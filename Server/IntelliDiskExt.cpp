/* Copyright (C) 2022-2026 Stefan-Mihai MOGA
This file is part of IntelliDisk application developed by Stefan-Mihai MOGA.
IntelliDisk is an alternative Windows version to the famous Microsoft OneDrive!

IntelliDisk is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Open
Source Initiative, either version 3 of the License, or any later version.

IntelliDisk is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
IntelliDisk. If not, see <http://www.opensource.org/licenses/gpl-3.0.html>*/

#include "pch.h"
#include "IntelliDiskExt.h"
#include "IntelliDiskINI.h"
#include "IntelliDiskSQL.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Protocol control characters for communication handshake
// Same as client-side protocol for bidirectional communication
#define STX 0x02  // Start of Text - marks the beginning of a data packet
#define ETX 0x03  // End of Text - marks the end of a data packet
#define EOT 0x04  // End of Transmission - signals end of communication
#define ENQ 0x05  // Enquiry - requests acknowledgment from receiver
#define ACK 0x06  // Acknowledgment - confirms successful receipt
#define NAK 0x15  // Negative Acknowledgment - indicates transmission error

// File event identifiers for client notification queue
#define ID_STOP_PROCESS 0x01   // Stop processing (not used server-side)
#define ID_FILE_DOWNLOAD 0x02  // Notify client to download file
#define ID_FILE_UPLOAD 0x03    // Notify client to upload file (not used)
#define ID_FILE_DELETE 0x04    // Notify client to delete file

constexpr auto NOTIFY_FILE_SIZE = 0x10000;      // Max queue size per client
constexpr auto MAX_SOCKET_CONNECTIONS = 0x10000; // Max concurrent clients

// === GLOBAL SERVER STATE ===
// Server lifecycle flag - controls all client threads
bool g_bServerRunning = false;

// Main server socket that accepts incoming connections
CWSocket g_pServerSocket;

// Client connection management
int g_nSocketCount = 0;  // Total number of connected clients
CWSocket g_pClientSocket[MAX_SOCKET_CONNECTIONS];  // Socket for each client
int g_nThreadCount = 0;  // Total number of active threads
DWORD m_dwThreadID[MAX_SOCKET_CONNECTIONS] = { 0, };  // Thread IDs
HANDLE g_hThreadArray[MAX_SOCKET_CONNECTIONS] = { nullptr, };  // Thread handles

// === PER-CLIENT NOTIFICATION QUEUE ARCHITECTURE ===
// Each connected client has its own notification queue to receive
// file change events from other clients (multi-client sync mechanism)

// Single file event item
typedef struct {
	int nFileEvent;           // Event type: ID_FILE_DOWNLOAD or ID_FILE_DELETE
	std::wstring strFilePath; // Affected file path
} NOTIFY_FILE_DATA;

// Complete notification queue for one client
// Uses producer-consumer pattern with circular buffer
typedef struct {
	HANDLE hOccupiedSemaphore;  // Count of items in queue
	HANDLE hEmptySemaphore;      // Count of free slots
	HANDLE hResourceMutex;       // Protects queue data structure
	int nNextIn;                 // Write pointer (circular)
	int nNextOut;                // Read pointer (circular)
	NOTIFY_FILE_DATA arrNotifyData[NOTIFY_FILE_SIZE];  // Circular buffer
} NOTIFY_FILE_ITEM;

// Array of notification queues - one per client
// g_pThreadData[i] corresponds to g_pClientSocket[i]
NOTIFY_FILE_ITEM* g_pThreadData[MAX_SOCKET_CONNECTIONS];

// === DATABASE AND AUTHENTICATION CONFIGURATION ===
// Loaded from IntelliDisk.xml at server startup
std::wstring g_strHostName;  // MySQL server hostname
int g_nHostPort = 3306;       // MySQL server port (default 3306)
std::wstring g_strDatabase;   // Database name
std::wstring g_strUsername;   // Database username
std::wstring g_strPassword;   // Database password

/**
 * @brief Converts a UTF-8 encoded std::string to std::wstring
 * @param string The UTF-8 encoded string to convert
 * @return The converted wide string
 * @throws std::runtime_error If the conversion fails
 */
std::wstring utf8_to_wstring(const std::string& string)
{
	if (string.empty())
		return L"";
	const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, string.data(), (int)string.size(), nullptr, 0);
	if (size_needed <= 0)
		throw std::runtime_error("MultiByteToWideChar() failed: " + std::to_string(size_needed));
	std::wstring result(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, string.data(), (int)string.size(), result.data(), size_needed);
	return result;
}

/**
 * @brief Converts a std::wstring to a UTF-8 encoded std::string
 * @param wide_string The wide string to convert
 * @return The UTF-8 encoded string
 * @throws std::runtime_error If the conversion fails
 */
std::string wstring_to_utf8(const std::wstring& wide_string)
{
	if (wide_string.empty())
		return "";
	const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_string.data(), (int)wide_string.size(), nullptr, 0, nullptr, nullptr);
	if (size_needed <= 0)
		throw std::runtime_error("WideCharToMultiByte() failed: " + std::to_string(size_needed));
	std::string result(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wide_string.data(), (int)wide_string.size(), result.data(), size_needed, nullptr, nullptr);
	return result;
}

const int MAX_BUFFER = 0x10000;
bool g_bIsConnected[MAX_SOCKET_CONNECTIONS];

// Hexadecimal conversion helpers
const char HEX_MAP[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

/**
 * @brief Maps a nibble to its hexadecimal character
 * @param c The byte value to convert (only lower 4 bits are used)
 * @return The hexadecimal character ('0'-'9', 'A'-'F')
 */
char replace(unsigned char c)
{
	return HEX_MAP[c & 0x0f];
}

/**
 * @brief Converts a byte to a two-character hexadecimal string
 * @param c The byte value to convert
 * @return Two-character hexadecimal string representation
 */
std::string char_to_hex(unsigned char c)
{
	std::string hex;
	char left = (c >> 4);
	char right = (c & 0x0f);
	hex += replace(left);
	hex += replace(right);
	return hex;
}

/**
 * @brief Dumps a buffer as a space-separated hexadecimal string
 * @param pBuffer Pointer to the buffer to dump
 * @param nLength Length of the buffer in bytes
 * @return Wide string containing space-separated hexadecimal values
 */
std::wstring dumpHEX(unsigned char* pBuffer, const int nLength)
{
	std::string result;
	for (int nIndex = 0; nIndex < nLength; nIndex++)
	{
		result += char_to_hex(pBuffer[nIndex]);
		result += " ";
	}
	return utf8_to_wstring(result);
}

/**
 * @brief Reads a buffer from the socket, handling protocol handshakes (ENQ, EOT, ACK/NAK)
 * @param nSocketIndex Index of the client socket in the global socket array
 * @param pApplicationSocket The socket to read from
 * @param pBuffer Buffer to store received data
 * @param nLength [in/out] Length of buffer and received data
 * @param ReceiveENQ Whether to expect an ENQ handshake
 * @param ReceiveEOT Whether to expect an EOT handshake
 * @return true on successful read and protocol validation, false otherwise
 */
bool ReadBuffer(const int nSocketIndex, CWSocket& pApplicationSocket, unsigned char* pBuffer, int& nLength, const bool ReceiveENQ, const bool ReceiveEOT)
{
	int nIndex = 0;
	int nCount = 0;
	char nReturn = ACK;

	try
	{
		if (ReceiveENQ)
		{
			nLength = MAX_BUFFER;
			if (pApplicationSocket.IsReadible(1000) &&
				((nLength = pApplicationSocket.Receive(pBuffer, nLength)) > 0) &&
				(ENQ == pBuffer[nLength - 1]))
			{
				TRACE(_T("ENQ Received\n"));
				unsigned char chACK = ACK;
				VERIFY(pApplicationSocket.Send(&chACK, sizeof(chACK)) == 1);
				TRACE(_T("ACK Sent\n"));
			}
			else
				return false;
		}
		nLength = 0;
		do {
			while (pApplicationSocket.IsReadible(1000) &&
				((nIndex = pApplicationSocket.Receive(pBuffer + nIndex, MAX_BUFFER - nLength)) > 0))
			{
				nLength += nIndex;
				TRACE(_T("Buffer Received %s\n"), dumpHEX(pBuffer, nLength).c_str());
				nReturn = (pBuffer[nLength - 1] == calcLRC(&pBuffer[3], (nLength - 5))) ? ACK : NAK;
			}
			VERIFY(pApplicationSocket.Send(&nReturn, sizeof(nReturn)) == 1);
			TRACE(_T("%s Sent\n"), ((ACK == nReturn) ? _T("ACK") : _T("NAK")));
		} while ((ACK != nReturn) && (++nCount < 3));
		if (ReceiveEOT)
		{
			unsigned char chEOT = ACK;
			if (pApplicationSocket.IsReadible(1000) &&
				((nLength = pApplicationSocket.Receive(&chEOT, sizeof(chEOT))) > 0) &&
				(EOT == chEOT))
			{
				TRACE(_T("EOT Received\n"));
			}
			else
				return false;
		}
	}
	catch (CWSocketException* pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		pException->Delete();
		pApplicationSocket.Close();
		g_bIsConnected[nSocketIndex] = false;
		return false;
	}
	TRACE(_T("ReadBuffer: %s\n"), ((ACK == nReturn) ? _T("true") : _T("false")));
	return (ACK == nReturn);
}

/**
 * @brief Writes a buffer to the socket, handling protocol handshakes (ENQ, EOT, ACK/NAK)
 * @param nSocketIndex Index of the client socket in the global socket array
 * @param pApplicationSocket The socket to write to
 * @param pBuffer Buffer containing data to send
 * @param nLength Length of data to send
 * @param SendENQ Whether to send an ENQ handshake
 * @param SendEOT Whether to send an EOT handshake
 * @return true on successful write and protocol validation, false otherwise
 */
#pragma warning(suppress: 6262)
bool WriteBuffer(const int nSocketIndex, CWSocket& pApplicationSocket, const unsigned char* pBuffer, const int nLength, const bool SendENQ, const bool SendEOT)
{
	int nCount = 0;
	unsigned char nReturn = ACK;
	unsigned char pPacket[MAX_BUFFER] = { 0, };

	try
	{
		ASSERT(nLength <= (sizeof(pPacket) - 5));
		if (SendENQ && pApplicationSocket.IsWritable(1000))
		{
			unsigned char chENQ = ENQ;
			TRACE(_T("ENQ Sent\n"));
			VERIFY(pApplicationSocket.Send(&chENQ, sizeof(chENQ)) == 1);
			ZeroMemory(pPacket, sizeof(pPacket));
			nCount = MAX_BUFFER;
			if (((nCount = pApplicationSocket.Receive(pPacket, nCount)) > 0) &&
				(ACK == pPacket[nCount - 1]))
			{
				TRACE(_T("ACK Received\n"));
				nCount = 0;
			}
			else
				return false;
		}
		ZeroMemory(pPacket, sizeof(pPacket));
		CopyMemory(&pPacket[3], pBuffer, nLength);
		pPacket[0] = STX;
		pPacket[1] = (char)(nLength / 0x100);
		pPacket[2] = nLength % 0x100;
		pPacket[3 + nLength] = ETX;
		pPacket[4 + nLength] = calcLRC(pBuffer, nLength);
		do {
			if (pApplicationSocket.Send(pPacket, (5 + nLength)) == (5 + nLength))
			{
				TRACE(_T("Buffer Sent %s\n"), dumpHEX(pPacket, (5 + nLength)).c_str());
				VERIFY(pApplicationSocket.Receive(&nReturn, sizeof(nReturn)) == 1);
				TRACE(_T("%s Received\n"), ((ACK == nReturn) ? _T("ACK") : _T("NAK")));
			}
		} while ((nReturn != ACK) && (++nCount <= 3));
		if (SendEOT && pApplicationSocket.IsWritable(1000))
		{
			unsigned char chEOT = EOT;
			TRACE(_T("EOT Sent\n"));
			VERIFY(pApplicationSocket.Send(&chEOT, sizeof(chEOT)) == 1);
		}
	}
	catch (CWSocketException* pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		pException->Delete();
		pApplicationSocket.Close();
		g_bIsConnected[nSocketIndex] = false;
		return false;
	}
	TRACE(_T("WriteBuffer: %s\n"), ((ACK == nReturn) ? _T("true") : _T("false")));
	return (ACK == nReturn);
}

/**
 * @brief Pushes a file event notification into the per-client queue
 * @param nSocketIndex Index of the client socket
 * @param nFileEvent The file event type (ID_FILE_UPLOAD, ID_FILE_DOWNLOAD, ID_FILE_DELETE)
 * @param strFilePath The file path associated with the event
 * 
 * MULTI-CLIENT SYNCHRONIZATION:
 * =============================
 * When Client A uploads/deletes a file, the server calls PushNotification()
 * for all OTHER clients (B, C, D...) to notify them of the change.
 * Each client's IntelliDiskThread will pop events from its queue and
 * send NotifyDownload or NotifyDelete commands to keep clients in sync.
 */
void PushNotification(const int& nSocketIndex, const int nFileEvent, const std::wstring& strFilePath)
{
	NOTIFY_FILE_ITEM* pThreadData = g_pThreadData[nSocketIndex];
	// Verify queue is initialized before accessing
	if ((pThreadData->hResourceMutex != nullptr) &&
		(pThreadData->hEmptySemaphore != nullptr) &&
		(pThreadData->hOccupiedSemaphore != nullptr))
	{
		// Wait for free space in queue
		WaitForSingleObject(pThreadData->hEmptySemaphore, INFINITE);
		// Lock queue for thread-safe access
		WaitForSingleObject(pThreadData->hResourceMutex, INFINITE);

		TRACE(_T("[PushNotification] nFileEvent = %d, strFilePath = \"%s\"\n"), nFileEvent, strFilePath.c_str());
		// Add item to circular queue
		pThreadData->arrNotifyData[pThreadData->nNextIn].nFileEvent = nFileEvent;
		pThreadData->arrNotifyData[pThreadData->nNextIn].strFilePath = strFilePath;
		pThreadData->nNextIn++;
		pThreadData->nNextIn %= NOTIFY_FILE_SIZE;  // Wrap around

		// Release lock and signal item available
		ReleaseSemaphore(pThreadData->hResourceMutex, 1, nullptr);
		ReleaseSemaphore(pThreadData->hOccupiedSemaphore, 1, nullptr);
	}
}

/**
 * @brief Pops a file event notification from the per-client queue
 * @param nSocketIndex Index of the client socket
 * @param nFileEvent [out] The file event type
 * @param strFilePath [out] The file path associated with the event
 */
void PopNotification(const int& nSocketIndex, int& nFileEvent, std::wstring& strFilePath)
{
	NOTIFY_FILE_ITEM* pThreadData = g_pThreadData[nSocketIndex];
	if ((pThreadData->hResourceMutex != nullptr) &&
		(pThreadData->hEmptySemaphore != nullptr) &&
		(pThreadData->hOccupiedSemaphore != nullptr))
	{
		// Wait for item to be available in queue
		WaitForSingleObject(pThreadData->hOccupiedSemaphore, INFINITE);
		// Lock queue for thread-safe access
		WaitForSingleObject(pThreadData->hResourceMutex, INFINITE);

		// Remove item from circular queue (FIFO)
		nFileEvent = pThreadData->arrNotifyData[pThreadData->nNextOut].nFileEvent;
		strFilePath = pThreadData->arrNotifyData[pThreadData->nNextOut].strFilePath;
		pThreadData->nNextOut++;
		pThreadData->nNextOut %= NOTIFY_FILE_SIZE;  // Wrap around
		TRACE(_T("[PopNotification] nFileEvent = %d, strFilePath = \"%s\"\n"), nFileEvent, strFilePath.c_str());

		// Release lock and signal free space available
		ReleaseSemaphore(pThreadData->hResourceMutex, 1, nullptr);
		ReleaseSemaphore(pThreadData->hEmptySemaphore, 1, nullptr);
	}
}

/**
 * @brief Main thread function for handling a single IntelliDisk client connection
 * @details Handles protocol negotiation, file commands (upload, download, delete),
 *          and notification queue processing for the client
 * @param lpParam Pointer to the socket index (int*)
 * @return 0 on thread exit
 * 
 * ARCHITECTURE: PER-CLIENT THREAD
 * ================================
 * Each connected client gets its own thread that:
 * 1. Manages client authentication (IntelliDisk + machine ID)
 * 2. Processes client-initiated commands (Upload, Download, Delete, Ping, Close)
 * 3. Monitors per-client notification queue for multi-client sync events
 * 4. Broadcasts file changes to other clients via PushNotification()
 * 
 * COMMAND PROTOCOL:
 * =================
 * Client -> Server:
 *   - "IntelliDisk" + MachineID: Initial handshake
 *   - "Upload" + filepath: Store file in database
 *   - "Download" + filepath: Retrieve file from database
 *   - "Delete" + filepath: Remove file from database
 *   - "Ping": Keep-alive message
 *   - "Close": Graceful disconnect
 * 
 * Server -> Client (Push Notifications):
 *   - "Restart": Server shutting down
 *   - "NotifyDownload" + filepath: Another client uploaded - download to sync
 *   - "NotifyDelete" + filepath: Another client deleted - delete to sync
 */
#pragma warning(suppress: 6262)
DWORD WINAPI IntelliDiskThread(LPVOID lpParam)
{
	unsigned char pBuffer[MAX_BUFFER] = { 0, };
	int nLength = 0;
	// Get socket index from parameter (passed by CreateDatabase)
	const int nSocketIndex = *(int*)(lpParam);
	TRACE(_T("nSocketIndex = %d\n"), (nSocketIndex));
	CWSocket& pApplicationSocket = g_pClientSocket[nSocketIndex];
	ASSERT(pApplicationSocket.IsCreated());
	std::wstring strComputerID;  // Client's unique machine identifier

	// === INITIALIZE PER-CLIENT NOTIFICATION QUEUE ===
	// Each client needs its own queue to receive sync notifications
	g_pThreadData[nSocketIndex] = new NOTIFY_FILE_ITEM;
	ASSERT(g_pThreadData[nSocketIndex] != nullptr);
	NOTIFY_FILE_ITEM* pThreadData = g_pThreadData[nSocketIndex];
	ASSERT(pThreadData != nullptr);
	// Create synchronization objects for producer-consumer queue
	pThreadData->hOccupiedSemaphore = CreateSemaphore(nullptr, 0, NOTIFY_FILE_SIZE, nullptr);  // Items in queue
	pThreadData->hEmptySemaphore = CreateSemaphore(nullptr, NOTIFY_FILE_SIZE, NOTIFY_FILE_SIZE, nullptr);  // Free slots
	pThreadData->hResourceMutex = CreateSemaphore(nullptr, 1, 1, nullptr);  // Queue mutex
	// Initialize circular queue pointers
	pThreadData->nNextIn = pThreadData->nNextOut = 0;

	g_bIsConnected[nSocketIndex] = false;

	while (g_bServerRunning)
	{
		try
		{
			if (!pApplicationSocket.IsCreated())
				break;  // Socket closed, exit thread

			if (pApplicationSocket.IsReadible(1000))
			{
				// === CLIENT COMMAND RECEIVED ===
				nLength = sizeof(pBuffer);
				ZeroMemory(pBuffer, sizeof(pBuffer));
				if (ReadBuffer(nSocketIndex, pApplicationSocket, pBuffer, nLength, true, false))
				{
					const std::string strCommand = (char*) &pBuffer[3];
					if (strCommand.compare("IntelliDisk") == 0)
					{
						// HANDSHAKE: Client sends "IntelliDisk" + machine ID for authentication
						TRACE(_T("Client connected!\n"));
						nLength = sizeof(pBuffer);
						ZeroMemory(pBuffer, sizeof(pBuffer));
						if (ReadBuffer(nSocketIndex, pApplicationSocket, pBuffer, nLength, false, true))
						{
							strComputerID = utf8_to_wstring((char*) &pBuffer[3]);
							TRACE(_T("Logged In: %s!\n"), strComputerID.c_str());
							g_bIsConnected[nSocketIndex] = true;
						}
					}
					else
					{
						if (strCommand.compare("Close") == 0)
						{
							// CLIENT DISCONNECT: Graceful shutdown
							nLength = sizeof(pBuffer);
							ZeroMemory(pBuffer, sizeof(pBuffer));
							if (((nLength = pApplicationSocket.Receive(pBuffer, nLength)) > 0) &&
								(EOT == pBuffer[nLength - 1]))
							{
								TRACE(_T("EOT Received\n"));
							}
							pApplicationSocket.Close();
							TRACE(_T("Logged Out: %s!\n"), strComputerID.c_str());
							break;
						}
						else
						{
							if (strCommand.compare("Ping") == 0)
							{
								nLength = sizeof(pBuffer);
								ZeroMemory(pBuffer, sizeof(pBuffer));
								if (((nLength = pApplicationSocket.Receive(pBuffer, nLength)) > 0) &&
									(EOT == pBuffer[nLength - 1]))
								{
									TRACE(_T("EOT Received\n"));
								}
								TRACE(_T("Ping!\n"));
							}
							else
							{
								if (strCommand.compare("Download") == 0)
								{
									nLength = sizeof(pBuffer);
									ZeroMemory(pBuffer, sizeof(pBuffer));
									if (ReadBuffer(nSocketIndex, pApplicationSocket, pBuffer, nLength, false, false))
									{
										const std::wstring& strFilePath = utf8_to_wstring((char*) &pBuffer[3]);
										TRACE(_T("Downloading %s...\n"), strFilePath.c_str());
										VERIFY(DownloadFile(nSocketIndex, pApplicationSocket, strFilePath));
									}
								}
								else
								{
									if (strCommand.compare("Upload") == 0)
									{
										nLength = sizeof(pBuffer);
										ZeroMemory(pBuffer, sizeof(pBuffer));
										if (ReadBuffer(nSocketIndex, pApplicationSocket, pBuffer, nLength, false, false))
										{
											const std::wstring& strFilePath = utf8_to_wstring((char*) &pBuffer[3]);
											TRACE(_T("Uploading %s...\n"), strFilePath.c_str());
											// Store file in MySQL database
											VERIFY(UploadFile(nSocketIndex, pApplicationSocket, strFilePath));

											// === BROADCAST TO ALL OTHER CLIENTS ===
											// Notify all other clients to download this file for sync
											for (int nThreadIndex = 0; nThreadIndex < g_nSocketCount; nThreadIndex++)
											{
												if (nThreadIndex != nSocketIndex) // Skip current client
													PushNotification(nThreadIndex, ID_FILE_DOWNLOAD, strFilePath);
											}
										}
									}
									else
									{
										if (strCommand.compare("Delete") == 0)
										{
											nLength = sizeof(pBuffer);
											ZeroMemory(pBuffer, sizeof(pBuffer));
											if (ReadBuffer(nSocketIndex, pApplicationSocket, pBuffer, nLength, false, false))
											{
												const std::wstring& strFilePath = utf8_to_wstring((char*) &pBuffer[3]);
												TRACE(_T("Deleting %s...\n"), strFilePath.c_str());
												// Remove file from MySQL database
												VERIFY(DeleteFile(nSocketIndex, pApplicationSocket, strFilePath));

												// === BROADCAST TO ALL OTHER CLIENTS ===
												// Notify all other clients to delete this file for sync
												for (int nThreadIndex = 0; nThreadIndex < g_nSocketCount; nThreadIndex++)
												{
													if (nThreadIndex != nSocketIndex) // Skip current client
														PushNotification(nThreadIndex, ID_FILE_DELETE, strFilePath);
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
			else
			{
				// === NO DATA AVAILABLE - CHECK NOTIFICATION QUEUE ===
				// If server is stopping, notify client to restart
				if (!g_bServerRunning)
				{
					// Send restart command to client for graceful shutdown
					const std::string strCommand = "Restart";
					nLength = (int)strCommand.length() + 1;
					if (WriteBuffer(nSocketIndex, pApplicationSocket, (unsigned char*)strCommand.c_str(), nLength, true, true))
					{
						TRACE(_T("Restart!\n"));
					}
				}
				else
				{
					// Process queued notifications for this client
					// Check if there are pending file sync notifications
					if (pThreadData->nNextIn != pThreadData->nNextOut)
					{
						// Dequeue notification and send to client
						int nFileEvent = 0;
						std::wstring strFilePath;
						PopNotification(nSocketIndex, nFileEvent, strFilePath);

						if (ID_FILE_DOWNLOAD == nFileEvent)
						{
							// Another client uploaded - tell this client to download
							const std::string strCommand = "NotifyDownload";
							nLength = (int)strCommand.length() + 1;
							if (WriteBuffer(nSocketIndex, pApplicationSocket, (unsigned char*)strCommand.c_str(), nLength, true, false))
							{
								const std::string strFileName = wstring_to_utf8(strFilePath);
								nLength = (int)strFileName.length() + 1;
								if (WriteBuffer(nSocketIndex, pApplicationSocket, (unsigned char*)strFileName.c_str(), nLength, false, false))
								{
									TRACE(_T("Downloading %s...\n"), strFilePath.c_str());
									VERIFY(DownloadFile(nSocketIndex, pApplicationSocket, strFilePath));
								}
							}
						}
						else if (ID_FILE_DELETE == nFileEvent)
						{
							const std::string strCommand = "NotifyDelete";
							nLength = (int)strCommand.length() + 1;
							if (WriteBuffer(nSocketIndex, pApplicationSocket, (unsigned char*)strCommand.c_str(), nLength, true, false))
							{
								const std::string strFileName = wstring_to_utf8(strFilePath);
								nLength = (int)strFileName.length() + 1;
								if (WriteBuffer(nSocketIndex, pApplicationSocket, (unsigned char*)strFileName.c_str(), nLength, false, false))
								{
									TRACE(_T("Deleting %s...\n"), strFilePath.c_str());
								}
							}
						}
					}
				}
			}
		}
		catch (CWSocketException* pException)
		{
			const int nErrorLength = 0x100;
			TCHAR lpszErrorMessage[nErrorLength] = { 0, };
			pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
			TRACE(_T("%s\n"), lpszErrorMessage);
			pException->Delete();
			pApplicationSocket.Close();
			g_bIsConnected[nSocketIndex] = false;
			break;
		}
	}

	// Cleanup per-thread resources
	if (pThreadData->hResourceMutex != nullptr)
	{
		VERIFY(CloseHandle(pThreadData->hResourceMutex));
		pThreadData->hResourceMutex = nullptr;
	}
	if (pThreadData->hEmptySemaphore != nullptr)
	{
		VERIFY(CloseHandle(pThreadData->hEmptySemaphore));
		pThreadData->hEmptySemaphore = nullptr;
	}
	if (pThreadData->hOccupiedSemaphore != nullptr)
	{
		VERIFY(CloseHandle(pThreadData->hOccupiedSemaphore));
		pThreadData->hOccupiedSemaphore = nullptr;
	}
	delete g_pThreadData[nSocketIndex];
	g_pThreadData[nSocketIndex] = nullptr;

	TRACE(_T("exiting...\n"));
	return 0;
}

int g_nServicePort = IntelliDiskPort;

/**
 * @brief Main server thread for accepting client connections and launching handler threads
 * @details Loads configuration from INI file, binds the server socket, and enters the accept loop.
 *          Creates a new IntelliDiskThread for each incoming client connection
 * @param lpParam Unused parameter
 * @return 0 on success, non-zero on error
 * 
 * SERVER LIFECYCLE:
 * =================
 * 1. Load service port and database settings from IntelliDisk.xml
 * 2. Bind server socket to port (default 8080)
 * 3. Listen for incoming connections (backlog = 65536)
 * 4. Accept loop:
 *    - Accept() blocks until client connects
 *    - Create new IntelliDiskThread for each client
 *    - Increment socket/thread counters
 * 5. Shutdown: Close server socket and wait for threads
 */
DWORD WINAPI CreateDatabase(LPVOID lpParam)
{
	UNREFERENCED_PARAMETER(lpParam);
	try
	{
		TRACE(_T("CreateDatabase()\n"));
		// Load configuration from XML settings file
		g_nServicePort = LoadServicePort();
		if (!LoadAppSettings(g_strHostName, g_nHostPort, g_strDatabase, g_strUsername, g_strPassword))
		{
			// Configuration load failed but continue with defaults
			// return (DWORD)-1;
		}

		// Bind server socket to port and start listening
		g_pServerSocket.CreateAndBind(g_nServicePort, SOCK_STREAM, AF_INET);
		if (g_pServerSocket.IsCreated())
		{
			g_bServerRunning = true;
			g_pServerSocket.Listen(MAX_SOCKET_CONNECTIONS);  // Backlog = 65536

			// === ACCEPT LOOP ===
			// Accept incoming connections until server stops
			while (g_bServerRunning)
			{
				// Block until client connects (or StopProcessingThread interrupts)
				g_pServerSocket.Accept(g_pClientSocket[g_nSocketCount]);

				if (g_bServerRunning)
				{
					// Create new thread to handle this client
					const int nSocketIndex = g_nSocketCount;
					g_hThreadArray[g_nThreadCount] = CreateThread(nullptr, 0, IntelliDiskThread, (int*)&nSocketIndex, 0, &m_dwThreadID[g_nThreadCount]);
					ASSERT(g_hThreadArray[g_nThreadCount] != nullptr);
					g_nSocketCount++;
					g_nThreadCount++;
				}
				else
				{
					// Server stopping - close the socket that unblocked Accept()
					g_pClientSocket[g_nSocketCount].Close();
					g_nSocketCount++;
				}
			}
		}
		g_pServerSocket.Close();
	}
	catch (CWSocketException* pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		pException->Delete();
	}
	return 0;
}

/**
 * @brief Starts the main server processing thread
 * @details Creates and launches the CreateDatabase thread which accepts client connections
 */
void StartProcessingThread()
{
	TRACE(_T("StartProcessingThread()\n"));
	g_hThreadArray[g_nThreadCount] = CreateThread(nullptr, 0, CreateDatabase, nullptr, 0, &m_dwThreadID[g_nThreadCount]);
	ASSERT(g_hThreadArray[g_nThreadCount] != nullptr);
	g_nThreadCount++;
}

/**
 * @brief Stops the server processing thread and closes all client sockets
 * @details Sets server running flag to false, connects to self to unblock accept(),
 *          waits for all threads to complete, and closes all client connections
 * 
 * GRACEFUL SHUTDOWN SEQUENCE:
 * ===========================
 * 1. Set g_bServerRunning = false to signal all threads to stop
 * 2. Connect to self (localhost) to unblock Accept() call
 * 3. Wait for all client threads to exit (they'll see g_bServerRunning == false)
 * 4. Close all client sockets
 * 5. Reset counters
 */
void StopProcessingThread()
{
	CWSocket pClosingSocket;
	try
	{
		TRACE(_T("StopProcessingThread()\n"));
		if (g_bServerRunning)
		{
			// Step 1: Signal all threads to stop
			g_bServerRunning = false;

			// Step 2: Unblock Accept() by connecting to ourselves
			pClosingSocket.CreateAndConnect(IntelliDiskIP, g_nServicePort);

			// Step 3: Wait for all threads to complete
			WaitForMultipleObjects(g_nThreadCount, g_hThreadArray, TRUE, INFINITE);

			// Step 4: Close unblocking socket
			pClosingSocket.Close();

			// Step 5: Close all client sockets and reset counters
			for (int nIndex = 0; nIndex < g_nSocketCount; nIndex++)
				g_pClientSocket[nIndex].Close();
			g_nSocketCount = 0;
			g_nThreadCount = 0;
		}
	}
	catch (CWSocketException* pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		pException->Delete();
	}
}
