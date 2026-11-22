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
#include "MainFrame.h"
#include <KnownFolders.h>
#include <shlobj.h>
#include "SHA256.h"

#define SECURITY_WIN32
#include "Security.h"
#pragma comment(lib, "Secur32.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/**
 * @brief Converts a UTF-8 encoded std::string to std::wstring
 * @param str The UTF-8 encoded string to convert
 * @return The converted wide string
 */
std::wstring utf8_to_wstring(const std::string& str)
{
	// convert UTF-8 string to wstring
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	return myconv.from_bytes(str);
}

/**
 * @brief Converts a std::wstring to a UTF-8 encoded std::string
 * @param str The wide string to convert
 * @return The UTF-8 encoded string
 */
std::string wstring_to_utf8(const std::wstring& str)
{
	// convert wstring to UTF-8 string
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	return myconv.to_bytes(str);
}

/**
 * @brief Encodes a file path by replacing the user's profile folder with "IntelliDisk\"
 * @param strResult The file path to encode
 * @return The encoded file path
 */
std::wstring encode_filepath(std::wstring strResult)
{
	const std::wstring strApplicationName = _T("IntelliDisk\\");
	const std::wstring strSpecialFolder = GetSpecialFolder();
	// Replace full path with application-relative path for cross-machine compatibility
	if (strResult.find(strSpecialFolder) == 0)
		return strResult.replace(0, strSpecialFolder.length(), strApplicationName);
	return strResult;
}

/**
 * @brief Decodes a file path by replacing "IntelliDisk\" with the user's profile folder
 * @param strResult The file path to decode
 * @return The decoded file path
 */
std::wstring decode_filepath(std::wstring strResult)
{
	const std::wstring strApplicationName = _T("IntelliDisk\\");
	const std::wstring strSpecialFolder = GetSpecialFolder();
	// Replace application-relative path with full local path
	if (strResult.find(strApplicationName) == 0)
		return strResult.replace(0, strApplicationName.length(), strSpecialFolder);
	return strResult;
}

/**
 * @brief Retrieves a unique machine identifier based on the current user and computer name
 * @return UTF-8 encoded string in the format "username:computername"
 */
const std::string GetMachineID()
{
	/* LINUX:
	#include <unistd.h>
	#include <limits.h>
	char hostname[HOST_NAME_MAX];
	char username[LOGIN_NAME_MAX];
	gethostname(hostname, HOST_NAME_MAX);
	getlogin_r(username, LOGIN_NAME_MAX);
	*/

	// Retrieve current user name (prefer UPN format if available)
	DWORD nLength = 0x100;
	TCHAR lpszUserName[0x100] = { 0, };
	if (GetUserNameEx(NameUserPrincipal, lpszUserName, &nLength))
	{
		lpszUserName[nLength] = 0;
		TRACE(_T("UserName = %s\n"), lpszUserName);
	}
	else
	{
		// Fall back to simple user name if UPN is not available
		nLength = 0x100;
		if (GetUserName(lpszUserName, &nLength) != 0)
		{
			lpszUserName[nLength] = 0;
			TRACE(_T("UserName = %s\n"), lpszUserName);
		}
	}

	// Retrieve computer name (prefer fully qualified DNS name if available)
	nLength = 0x100;
	TCHAR lpszComputerName[0x100] = { 0, };
	if (GetComputerNameEx(ComputerNamePhysicalDnsFullyQualified, lpszComputerName, &nLength))
	{
		lpszComputerName[nLength] = 0;
		TRACE(_T("ComputerName = %s\n"), lpszComputerName);
	}
	else
	{
		// Fall back to NetBIOS name if DNS name is not available
		nLength = 0x100;
		if (GetComputerName(lpszComputerName, &nLength) != 0)
		{
			lpszComputerName[nLength] = 0;
			TRACE(_T("ComputerName = %s\n"), lpszComputerName);
		}
	}

	// Combine username and computername to create unique machine identifier
	std::wstring result(lpszUserName);
	result += _T(":");
	result += lpszComputerName;
	return wstring_to_utf8(result);
}

/**
 * @brief Gets the path to the user's profile folder and appends "IntelliDisk\"
 * @return The path to the IntelliDisk folder in the user's profile
 */
const std::wstring GetSpecialFolder()
{
	WCHAR* lpszSpecialFolderPath = nullptr;
	if ((SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &lpszSpecialFolderPath)) == S_OK)
	{
		std::wstring result(lpszSpecialFolderPath);
		CoTaskMemFree(lpszSpecialFolderPath);
		result += _T("\\IntelliDisk\\");
		return result;
	}
	return _T("");
}

/**
 * @brief Installs or removes IntelliDisk from Windows startup applications
 * @param bInstallStartupApps If true, adds to startup; if false, removes
 * @return true on success, false otherwise
 */
bool InstallStartupApps(bool bInstallStartupApps)
{
	HKEY regValue;
	bool result = false;
	// Open the Run registry key for current user
	if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_ALL_ACCESS, &regValue) == ERROR_SUCCESS)
	{
		if (bInstallStartupApps) // Add program to Startup Apps
		{
			TCHAR lpszApplicationBuffer[MAX_PATH + 1] = { 0, };
			if (GetModuleFileName(nullptr, lpszApplicationBuffer, MAX_PATH) > 0)
			{
				// Wrap path in quotes to handle spaces
				std::wstring quoted(_T("\""));
				quoted += lpszApplicationBuffer;
				quoted += _T("\"");
				const size_t length = quoted.length() * sizeof(TCHAR);
				// Set registry value to launch on startup
				if (RegSetValueEx(regValue, _T("IntelliDisk"), 0, REG_SZ, (LPBYTE)quoted.c_str(), (DWORD)length) == ERROR_SUCCESS)
				{
					result = true;
				}
			}
		}
		else // Remove program from Startup Apps
		{
			// Delete registry value to remove from startup
			if (RegDeleteValue(regValue, _T("IntelliDisk")) == ERROR_SUCCESS)
			{
				result = true;
			}
		}
		RegCloseKey(regValue);
	}
	return result;
}

/**
 * @brief Directory monitoring callback for file events
 * @param fiObject File information object containing details about the file
 * @param faAction The file action that occurred (create, delete, modify, etc.)
 * @param lpData User-defined data pointer (CMainFrame instance)
 * @return 0 on success
 */
UINT DirCallback(CFileInformation fiObject, EFileAction faAction, LPVOID lpData)
{
	// Determine event type based on file action (delete vs create/modify)
	const int nFileEvent = (IS_DELETE_FILE(faAction)) ? ID_FILE_DELETE : ID_FILE_UPLOAD;
	const std::wstring strFilePath = fiObject.GetFilePath().GetBuffer();
	// Queue the file event for processing by consumer thread
	AddNewItem(nFileEvent, strFilePath, lpData);

	return 0; // success
}

const int MAX_BUFFER = 0x10000; ///< Maximum buffer size for file and socket operations.
int g_nPingCount = 0;           ///< Ping counter for connection keep-alive.
bool g_bClientRunning = true;   ///< Global flag to control client threads.
bool g_bIsConnected = false;    ///< Global flag indicating connection status.

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
	// First four bits (high nibble)
	char left = (c >> 4);
	// Second four bits (low nibble)
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
 * @param pApplicationSocket The socket to read from
 * @param pBuffer Buffer to store received data
 * @param nLength [in/out] Length of buffer and received data
 * @param ReceiveENQ Whether to expect an ENQ handshake
 * @param ReceiveEOT Whether to expect an EOT handshake
 * @return true on successful read and protocol validation, false otherwise
 */
bool ReadBuffer(CWSocket& pApplicationSocket, unsigned char* pBuffer, int& nLength, const bool ReceiveENQ, const bool ReceiveEOT)
{
	int nIndex = 0;
	int nCount = 0;
	char nReturn = ACK;

	try
	{
		// Step 1: Handle ENQ (enquiry) handshake if requested
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
		// Step 2: Read data packet with retry on checksum failure
		nLength = 0;
		do {
			while (pApplicationSocket.IsReadible(1000) &&
				((nIndex = pApplicationSocket.Receive(pBuffer + nIndex, MAX_BUFFER - nLength)) > 0))
			{
				nLength += nIndex;
				TRACE(_T("Buffer Received %s\n"), dumpHEX(pBuffer, nLength).c_str());
				// Verify LRC (Longitudinal Redundancy Check) checksum
				// Note: calcLRC is defined in framework.h as inline function
				nReturn = (pBuffer[nLength - 1] == calcLRC(&pBuffer[3], (nLength - 5))) ? ACK : NAK;
			}
			VERIFY(pApplicationSocket.Send(&nReturn, sizeof(nReturn)) == 1);
			TRACE(_T("%s Sent\n"), ((ACK == nReturn) ? _T("ACK") : _T("NAK")));
		} while ((ACK != nReturn) && (++nCount < 3)); // Retry up to 3 times
		// Step 3: Handle EOT (end of transmission) if requested
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
		g_bIsConnected = false;
		return false;
	}
	TRACE(_T("ReadBuffer: %s\n"), ((ACK == nReturn) ? _T("true") : _T("false")));
	return (ACK == nReturn);
}

/**
 * @brief Writes a buffer to the socket, handling protocol handshakes (ENQ, EOT, ACK/NAK)
 * @param pApplicationSocket The socket to write to
 * @param pBuffer Buffer containing data to send
 * @param nLength Length of data to send
 * @param SendENQ Whether to send an ENQ handshake
 * @param SendEOT Whether to send an EOT handshake
 * @return true on successful write and protocol validation, false otherwise
 */
#pragma warning(suppress: 6262)
bool WriteBuffer(CWSocket& pApplicationSocket, const unsigned char* pBuffer, const int nLength, const bool SendENQ, const bool SendEOT)
{
	int nCount = 0;
	unsigned char nReturn = ACK;
	unsigned char pPacket[MAX_BUFFER] = { 0, };

	try
	{
		ASSERT(nLength <= (sizeof(pPacket) - 5));
		// Step 1: Send ENQ (enquiry) and wait for ACK if requested
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
		// Step 2: Prepare packet with STX header, data, ETX trailer, and LRC checksum
		ZeroMemory(pPacket, sizeof(pPacket));
		CopyMemory(&pPacket[3], pBuffer, nLength);
		pPacket[0] = STX;  // Start of text marker
		pPacket[1] = (char)(nLength / 0x100);  // Length high byte
		pPacket[2] = nLength % 0x100;  // Length low byte
		pPacket[3 + nLength] = ETX;  // End of text marker
		pPacket[4 + nLength] = calcLRC(pBuffer, nLength);  // Checksum
		// Step 3: Send packet and retry if NAK received
		do {
			if (pApplicationSocket.Send(pPacket, (5 + nLength)) == (5 + nLength))
			{
				TRACE(_T("Buffer Sent %s\n"), dumpHEX(pPacket, (5 + nLength)).c_str());
				VERIFY(pApplicationSocket.Receive(&nReturn, sizeof(nReturn)) == 1);
				TRACE(_T("%s Received\n"), ((ACK == nReturn) ? _T("ACK") : _T("NAK")));
			}
		} while ((nReturn != ACK) && (++nCount <= 3));  // Retry up to 3 times
		// Step 4: Send EOT (end of transmission) if requested
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
		g_bIsConnected = false;
		return false;
	}
	TRACE(_T("WriteBuffer: %s\n"), ((ACK == nReturn) ? _T("true") : _T("false")));
	return (ACK == nReturn);
}

std::wstring g_strCurrentDocument; ///< Currently processed document path (for upload/download).

/**
 * @brief Downloads a file from the server using the application socket
 * @details Verifies file integrity using SHA256 hash comparison
 * @param pApplicationSocket The socket to use for communication
 * @param strFilePath The local file path to save to
 * @return true on success, false otherwise
 */
#pragma warning(suppress: 6262)
bool DownloadFile(CWSocket& pApplicationSocket, const std::wstring& strFilePath)
{
	SHA256 pSHA256;
	unsigned char pFileBuffer[MAX_BUFFER] = { 0, };
	try
	{
		g_strCurrentDocument = strFilePath;
		TRACE(_T("[DownloadFile] %s\n"), strFilePath.c_str());
		CFile pBinaryFile(strFilePath.c_str(), CFile::modeWrite | CFile::modeCreate | CFile::typeBinary);
		ULONGLONG nFileLength = 0;
		int nLength = (int)(sizeof(nFileLength) + 5);
		ZeroMemory(pFileBuffer, sizeof(pFileBuffer));
		if (ReadBuffer(pApplicationSocket, pFileBuffer, nLength, false, false))
		{
			CopyMemory(&nFileLength, &pFileBuffer[3], sizeof(nFileLength));
			TRACE(_T("nFileLength = %llu\n"), nFileLength);

			// Read file data in chunks and write to local file
			ULONGLONG nFileIndex = 0;
			while (nFileIndex < nFileLength)
			{
				nLength = (int)sizeof(pFileBuffer);
				ZeroMemory(pFileBuffer, sizeof(pFileBuffer));
				if (ReadBuffer(pApplicationSocket, pFileBuffer, nLength, false, false))
				{
					nFileIndex += (nLength - 5);
					// Update SHA256 hash for integrity verification
					pSHA256.update(&pFileBuffer[3], nLength - 5);

					pBinaryFile.Write(&pFileBuffer[3], nLength - 5);
				}
			}
		}
		else
		{
			TRACE(_T("Invalid nFileLength!\n"));
			pBinaryFile.Close();
			return false;
		}
		// Verify file integrity using SHA256 hash
		const std::string strDigestSHA256 = pSHA256.toString(pSHA256.digest());
		nLength = (int)strDigestSHA256.length() + 5;
		ZeroMemory(pFileBuffer, sizeof(pFileBuffer));
		if (ReadBuffer(pApplicationSocket, pFileBuffer, nLength, false, true))
		{
			const std::string strCommand = (char*)&pFileBuffer[3];
			if (strDigestSHA256.compare(strCommand) != 0)
			{
				TRACE(_T("Invalid SHA256!\n"));
				pBinaryFile.Close();
				return false;
			}
		}
		pBinaryFile.Close();
		g_strCurrentDocument.clear();
	}
	catch (CFileException* pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		pException->Delete();
		return false;
	}
	return true;
}

/**
 * @brief Uploads a file to the server using the application socket
 * @details Sends file data and SHA256 digest for integrity verification
 * @param pApplicationSocket The socket to use for communication
 * @param strFilePath The local file path to upload
 * @return true on success, false otherwise
 */
#pragma warning(suppress: 6262)
bool UploadFile(CWSocket& pApplicationSocket, const std::wstring& strFilePath)
{
	SHA256 pSHA256;
	unsigned char pFileBuffer[MAX_BUFFER] = { 0, };
	try
	{
		TRACE(_T("[UploadFile] %s\n"), strFilePath.c_str());
		CFile pBinaryFile(strFilePath.c_str(), CFile::modeRead | CFile::typeBinary);
		// Send file length first
		ULONGLONG nFileLength = pBinaryFile.GetLength();
		int nLength = sizeof(nFileLength);
		if (WriteBuffer(pApplicationSocket, (unsigned char*)&nFileLength, nLength, false, false))
		{
			// Send file data in chunks
			ULONGLONG nFileIndex = 0;
			while (nFileIndex < nFileLength)
			{
				nLength = pBinaryFile.Read(pFileBuffer, MAX_BUFFER - 5);
				nFileIndex += nLength;
				if (WriteBuffer(pApplicationSocket, pFileBuffer, nLength, false, false))
				{
					// Update SHA256 hash as we send
					pSHA256.update(pFileBuffer, nLength);
				}
				else
				{
					pBinaryFile.Close();
					return false;
				}
			}
		}
		else
		{
			TRACE(_T("Invalid nFileLength!\n"));
			pBinaryFile.Close();
			return false;
		}
		// Send SHA256 digest for server-side verification
		const std::string strDigestSHA256 = pSHA256.toString(pSHA256.digest());
		nLength = (int)strDigestSHA256.length() + 1;
		if (WriteBuffer(pApplicationSocket, (unsigned char*)strDigestSHA256.c_str(), nLength, false, true))
		{
			TRACE(_T("Upload Done!\n"));
		}
		else
		{
			pBinaryFile.Close();
			return false;
		}
		pBinaryFile.Close();
	}
	catch (CFileException* pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		pException->Delete();
		return false;
	}
	return true;
}

/**
 * @brief Producer thread function
 * @details Handles connection establishment, login, and incoming server commands (Restart, NotifyDownload, NotifyDelete)
 * @param lpParam Pointer to CMainFrame instance
 * @return 0 on thread exit
 * 
 * ARCHITECTURE: Producer-Consumer Pattern
 * ========================================
 * This thread acts as the PRODUCER that:
 * 1. Maintains persistent connection to server
 * 2. Receives asynchronous commands from server (push notifications)
 * 3. Sends periodic keep-alive pings (60-second intervals)
 * 4. Handles connection state transitions and reconnection logic
 * 
 * The consumer thread (ConsumerThread) processes client-initiated file operations.
 * Both threads share the socket via mutex synchronization (hSocketMutex).
 * 
 * CONNECTION STATE MACHINE:
 * -------------------------
 * State 1: Disconnected -> Attempt connection + send "IntelliDisk" + machine ID
 * State 2: Connected -> Listen for server commands OR send periodic ping
 * State 3: Error -> Close socket, set disconnected flag, retry after 1 second
 */
#pragma warning(suppress: 6262)
DWORD WINAPI ProducerThread(LPVOID lpParam)
{
	unsigned char pBuffer[MAX_BUFFER] = { 0, };
	int nLength = 0;

	CMainFrame* pMainFrame = (CMainFrame*)lpParam;
	CWSocket& pApplicationSocket = pMainFrame->m_pApplicationSocket;
	HANDLE& hSocketMutex = pMainFrame->m_hSocketMutex;

	while (g_bClientRunning)
	{
		try
		{
			WaitForSingleObject(hSocketMutex, INFINITE);

			if (!pApplicationSocket.IsCreated())
			{
				// CONNECTION ESTABLISHMENT PROTOCOL:
				// Step 1: Create socket and connect to server
				pApplicationSocket.CreateAndConnect(pMainFrame->m_strServerIP, pMainFrame->m_nServerPort);
				// Step 2: Send application identifier "IntelliDisk"
				const std::string strCommand = "IntelliDisk";
				nLength = (int)strCommand.length() + 1;
				if (WriteBuffer(pApplicationSocket, (unsigned char*)strCommand.c_str(), nLength, true, false))
				{
					TRACE(_T("Client connected!\n"));
					// Step 3: Send unique machine identifier for authentication
					const std::string strMachineID = GetMachineID();
					int nComputerLength = (int)strMachineID.length() + 1;
					if (WriteBuffer(pApplicationSocket, (unsigned char*)strMachineID.c_str(), nComputerLength, false, true))
					{
						TRACE(_T("Logged In!\n"));
						g_bIsConnected = true;
						MessageBeep(MB_OK);
					}
				}
			}
			else
			{
				if (pApplicationSocket.IsReadible(1000))
				{
					// Reset ping counter when we receive data from server
					g_nPingCount = 0;
					nLength = sizeof(pBuffer);
					ZeroMemory(pBuffer, sizeof(pBuffer));
					if (ReadBuffer(pApplicationSocket, pBuffer, nLength, true, false))
					{
						// SERVER COMMAND PROTOCOL (Push notifications from server):
						const std::string strCommand = (char*)&pBuffer[3];
						TRACE(_T("strCommand = %s\n"), utf8_to_wstring(strCommand).c_str());
						if (strCommand.compare("Restart") == 0)
						{
							// Server is restarting - close connection and retry
							nLength = sizeof(pBuffer);
							ZeroMemory(pBuffer, sizeof(pBuffer));
							if (((nLength = pApplicationSocket.Receive(pBuffer, nLength)) > 0) &&
								(EOT == pBuffer[nLength - 1]))
							{
								TRACE(_T("EOT Received\n"));
							}
							TRACE(_T("Restart!\n"));
							pApplicationSocket.Close();
							g_bIsConnected = false;
						}
						else if (strCommand.compare("NotifyDownload") == 0)
						{
							// Another client uploaded a file - download it to sync
							nLength = sizeof(pBuffer);
							ZeroMemory(pBuffer, sizeof(pBuffer));
							if (ReadBuffer(pApplicationSocket, pBuffer, nLength, false, false))
							{
								CString strMessage;
								const std::wstring strUNICODE = decode_filepath(utf8_to_wstring((char*)&pBuffer[3]));
								strMessage.Format(_T("Downloading %s..."), strUNICODE.c_str());
								pMainFrame->ShowMessage(strMessage.GetBuffer(), strUNICODE);
								strMessage.ReleaseBuffer();

								TRACE(_T("Downloading %s...\n"), strUNICODE.c_str());
								VERIFY(DownloadFile(pApplicationSocket, strUNICODE));
							}
						}
						else if (strCommand.compare("NotifyDelete") == 0)
						{
							// Another client deleted a file - delete it locally to sync
							nLength = sizeof(pBuffer);
							ZeroMemory(pBuffer, sizeof(pBuffer));
							if (ReadBuffer(pApplicationSocket, pBuffer, nLength, false, false))
							{
								const std::wstring strUNICODE = decode_filepath(utf8_to_wstring((char*)&pBuffer[3]));
								VERIFY(DeleteFile(strUNICODE.c_str()));
							}
						}
					}
				}
				else
				{
					// No data available - check if we need to send keep-alive ping
					if (60 == ++g_nPingCount) // every 60 seconds
					{
						// Send ping to keep connection alive and detect dead connections
						g_nPingCount = 0;
						const std::string strCommand = "Ping";
						nLength = (int)strCommand.length() + 1;
						if (WriteBuffer(pApplicationSocket, (unsigned char*)strCommand.c_str(), nLength, true, true))
						{
							TRACE(_T("Ping!\n"));
						}
					}
				}
			}

			ReleaseSemaphore(hSocketMutex, 1, nullptr);
		}
		catch (CWSocketException* pException)
		{
			const int nErrorLength = 0x100;
			TCHAR lpszErrorMessage[nErrorLength] = { 0, };
			pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
			TRACE(_T("%s\n"), lpszErrorMessage);
			pException->Delete();
			ReleaseSemaphore(hSocketMutex, 1, nullptr);
			pApplicationSocket.Close();
			g_bIsConnected = false;
			Sleep(1000);
			continue;
		}
	}
	TRACE(_T("exiting...\n"));
	return 0;
}

/**
 * @brief Consumer thread function
 * @details Processes file events (upload, download, delete) from the resource queue and sends them to the server
 * @param lpParam Pointer to CMainFrame instance
 * @return 0 on thread exit
 * 
 * ARCHITECTURE: Producer-Consumer Pattern
 * ========================================
 * This thread acts as the CONSUMER that:
 * 1. Dequeues file events from the notification queue (FIFO)
 * 2. Processes client-initiated operations (upload, download, delete)
 * 3. Sends commands to server and handles file transfers
 * 4. Displays progress messages to user via main window
 * 
 * SYNCHRONIZATION PATTERN:
 * ------------------------
 * - hOccupiedSemaphore: Signals items available in queue (producer signals, consumer waits)
 * - hEmptySemaphore: Signals space available in queue (consumer signals, producer waits)
 * - hResourceMutex: Protects queue data structure from concurrent access
 * - hSocketMutex: Protects socket from concurrent use by producer thread
 * 
 * PROTOCOL COMMANDS TO SERVER:
 * ----------------------------
 * - "Close": Graceful shutdown (ID_STOP_PROCESS)
 * - "Download": Request file from server (ID_FILE_DOWNLOAD)
 * - "Upload": Send file to server (ID_FILE_UPLOAD)
 * - "Delete": Remove file from server (ID_FILE_DELETE)
 */
DWORD WINAPI ConsumerThread(LPVOID lpParam)
{
	int nLength = 0;

	CMainFrame* pMainFrame = (CMainFrame*)lpParam;
	HANDLE& hOccupiedSemaphore = pMainFrame->m_hOccupiedSemaphore;
	HANDLE& hEmptySemaphore = pMainFrame->m_hEmptySemaphore;
	HANDLE& hResourceMutex = pMainFrame->m_hResourceMutex;
	HANDLE& hSocketMutex = pMainFrame->m_hSocketMutex;

	CWSocket& pApplicationSocket = pMainFrame->m_pApplicationSocket;

	while (g_bClientRunning)
	{
		// === PHASE 1: DEQUEUE FILE EVENT ===
		// Wait for item to be available in queue
		WaitForSingleObject(hOccupiedSemaphore, INFINITE);
		// Lock queue for thread-safe access
		WaitForSingleObject(hResourceMutex, INFINITE);

		// Remove item from circular queue (FIFO)
		const int nFileEvent = pMainFrame->m_pResourceArray[pMainFrame->m_nNextOut].nFileEvent;
		const std::wstring& strFilePath = pMainFrame->m_pResourceArray[pMainFrame->m_nNextOut].strFilePath;
		TRACE(_T("[ConsumerThread] nFileEvent = %d, strFilePath = \"%s\"\n"), nFileEvent, strFilePath.c_str());
		pMainFrame->m_nNextOut++;
		pMainFrame->m_nNextOut %= NOTIFY_FILE_SIZE;

		// === PHASE 2: UPDATE UI WITH FILE EVENT ===
		if (ID_STOP_PROCESS == nFileEvent)
		{
			TRACE(_T("Stopping...\n"));
			g_bClientRunning = false;
		}
		else if (ID_FILE_DOWNLOAD == nFileEvent)
		{
			CString strMessage;
			strMessage.Format(_T("Downloading %s..."), strFilePath.c_str());
			pMainFrame->ShowMessage(strMessage.GetBuffer(), strFilePath);
			strMessage.ReleaseBuffer();
		}
		else if (ID_FILE_UPLOAD == nFileEvent)
		{
			CString strMessage;
			strMessage.Format(_T("Uploading %s..."), strFilePath.c_str());
			pMainFrame->ShowMessage(strMessage.GetBuffer(), strFilePath);
			strMessage.ReleaseBuffer();
		}
		else if (ID_FILE_DELETE == nFileEvent)
		{
			CString strMessage;
			strMessage.Format(_T("Deleting %s..."), strFilePath.c_str());
			pMainFrame->ShowMessage(strMessage.GetBuffer(), strFilePath);
			strMessage.ReleaseBuffer();
		}

		// Release queue locks before network I/O
		ReleaseSemaphore(hResourceMutex, 1, nullptr);
		ReleaseSemaphore(hEmptySemaphore, 1, nullptr);

		// === PHASE 3: WAIT FOR CONNECTION ===
		// Block until producer thread establishes connection
		while (g_bClientRunning && !g_bIsConnected);

		// === PHASE 4: SEND COMMAND TO SERVER ===
		// Lock socket for exclusive access
		WaitForSingleObject(hSocketMutex, INFINITE);

		try
		{
			if (pApplicationSocket.IsWritable(1000))
			{
				if (ID_STOP_PROCESS == nFileEvent)
				{
					const std::string strCommand = "Close";
					nLength = (int)strCommand.length() + 1;
					if (WriteBuffer(pApplicationSocket, (unsigned char*)strCommand.c_str(), nLength, true, true))
					{
						TRACE("Closing...\n");
						pApplicationSocket.Close();
					}
				}
				else
				{
					if (ID_FILE_DOWNLOAD == nFileEvent)
					{
						const std::string strCommand = "Download";
						nLength = (int)strCommand.length() + 1;
						if (WriteBuffer(pApplicationSocket, (unsigned char*)strCommand.c_str(), nLength, true, false))
						{
							const std::string strASCII = wstring_to_utf8(encode_filepath(strFilePath));
							const int nFileNameLength = (int)strASCII.length() + 1;
							if (WriteBuffer(pApplicationSocket, (unsigned char*)strASCII.c_str(), nFileNameLength, false, false))
							{
								TRACE(_T("Downloading %s...\n"), strFilePath.c_str());
								VERIFY(DownloadFile(pApplicationSocket, strFilePath));
							}
						}
					}
					else
					{
						if (ID_FILE_UPLOAD == nFileEvent)
						{
							const std::string strCommand = "Upload";
							nLength = (int)strCommand.length() + 1;
							if (WriteBuffer(pApplicationSocket, (unsigned char*)strCommand.c_str(), nLength, true, false))
							{
								const std::string strASCII = wstring_to_utf8(encode_filepath(strFilePath));
								const int nFileNameLength = (int)strASCII.length() + 1;
								if (WriteBuffer(pApplicationSocket, (unsigned char*)strASCII.c_str(), nFileNameLength, false, false))
								{
									TRACE(_T("Uploading %s...\n"), strFilePath.c_str());
									VERIFY(UploadFile(pApplicationSocket, strFilePath));
								}
							}
						}
						else
						{
							if (ID_FILE_DELETE == nFileEvent)
							{
								const std::string strCommand = "Delete";
								nLength = (int)strCommand.length() + 1;
								if (WriteBuffer(pApplicationSocket, (unsigned char*)strCommand.c_str(), nLength, true, false))
								{
									const std::string strASCII = wstring_to_utf8(encode_filepath(strFilePath));
									const int nFileNameLength = (int)strASCII.length() + 1;
									if (WriteBuffer(pApplicationSocket, (unsigned char*)strASCII.c_str(), nFileNameLength, false, true))
									{
										TRACE(_T("Deleting %s...\n"), strFilePath.c_str());
									}
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
			g_bIsConnected = false;
		}

		ReleaseSemaphore(hSocketMutex, 1, nullptr);
	}
	return 0;
}

/**
 * @brief Adds a new file event item to the resource queue for processing
 * @param nFileEvent The file event type (ID_FILE_UPLOAD, ID_FILE_DOWNLOAD, ID_FILE_DELETE, ID_STOP_PROCESS)
 * @param strFilePath The file path associated with the event
 * @param lpParam Pointer to CMainFrame instance
 */
void AddNewItem(const int nFileEvent, const std::wstring& strFilePath, LPVOID lpParam)
{
	// Avoid re-uploading file that's currently being downloaded
	if ((g_strCurrentDocument.compare(strFilePath) == 0) && (ID_FILE_UPLOAD == nFileEvent))
	{
		return;
	}

	CMainFrame* pMainFrame = (CMainFrame*)lpParam;
	HANDLE& hOccupiedSemaphore = pMainFrame->m_hOccupiedSemaphore;
	HANDLE& hEmptySemaphore = pMainFrame->m_hEmptySemaphore;
	HANDLE& hResourceMutex = pMainFrame->m_hResourceMutex;

	// Wait for available space in queue
	WaitForSingleObject(hEmptySemaphore, INFINITE);
	// Lock queue for thread-safe access
	WaitForSingleObject(hResourceMutex, INFINITE);

	TRACE(_T("[AddNewItem] nFileEvent = %d, strFilePath = \"%s\"\n"), nFileEvent, strFilePath.c_str());
	// Add item to circular queue
	pMainFrame->m_pResourceArray[pMainFrame->m_nNextIn].nFileEvent = nFileEvent;
	pMainFrame->m_pResourceArray[pMainFrame->m_nNextIn].strFilePath = strFilePath;
	pMainFrame->m_nNextIn++;
	pMainFrame->m_nNextIn %= NOTIFY_FILE_SIZE;  // Wrap around for circular queue

	// Release lock and signal that item is available
	ReleaseSemaphore(hResourceMutex, 1, nullptr);
	ReleaseSemaphore(hOccupiedSemaphore, 1, nullptr);
}
