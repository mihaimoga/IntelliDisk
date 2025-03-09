/* Copyright (C) 2022-2025 Stefan-Mihai MOGA
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

#define STX 0x02
#define ETX 0x03
#define EOT 0x04
#define ENQ 0x05
#define ACK 0x06
#define NAK 0x15

#define ID_STOP_PROCESS 0x01
#define ID_FILE_DOWNLOAD 0x02
#define ID_FILE_UPLOAD 0x03
#define ID_FILE_DELETE 0x04

constexpr auto NOTIFY_FILE_SIZE = 0x10000;

constexpr auto  MAX_SOCKET_CONNECTIONS = 0x10000;

bool g_bServerRunning = false;
CWSocket g_pServerSocket;
int g_nSocketCount = 0;
CWSocket g_pClientSocket[MAX_SOCKET_CONNECTIONS];
int g_nThreadCount = 0;
DWORD m_dwThreadID[MAX_SOCKET_CONNECTIONS];
HANDLE g_hThreadArray[MAX_SOCKET_CONNECTIONS];

typedef struct {
	int nFileEvent;
	std::wstring strFilePath;
} NOTIFY_FILE_DATA;

typedef struct {
	HANDLE hOccupiedSemaphore;
	HANDLE hEmptySemaphore;
	HANDLE hResourceMutex;
	int nNextIn;
	int nNextOut;
	NOTIFY_FILE_DATA arrNotifyData[NOTIFY_FILE_SIZE];
} NOTIFY_FILE_ITEM;

NOTIFY_FILE_ITEM* g_pThreadData[MAX_SOCKET_CONNECTIONS];

std::wstring g_strHostName;
int g_nHostPort = 3306;
std::wstring g_strDatabase;
std::wstring g_strUsername;
std::wstring g_strPassword;

std::wstring utf8_to_wstring(const std::string& string)
{
	if (string.empty())
	{
		return L"";
	}

	const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, string.data(), (int)string.size(), nullptr, 0);
	if (size_needed <= 0)
	{
		throw std::runtime_error("MultiByteToWideChar() failed: " + std::to_string(size_needed));
	}

	std::wstring result(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, string.data(), (int)string.size(), result.data(), size_needed);
	return result;
}

std::string wstring_to_utf8(const std::wstring& wide_string)
{
	if (wide_string.empty())
	{
		return "";
	}

	const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_string.data(), (int)wide_string.size(), nullptr, 0, nullptr, nullptr);
	if (size_needed <= 0)
	{
		throw std::runtime_error("WideCharToMultiByte() failed: " + std::to_string(size_needed));
	}

	std::string result(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wide_string.data(), (int)wide_string.size(), result.data(), size_needed, nullptr, nullptr);
	return result;
}

const int MAX_BUFFER = 0x10000;
bool g_bIsConnected[MAX_SOCKET_CONNECTIONS];

const char HEX_MAP[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
char replace(unsigned char c)
{
	return HEX_MAP[c & 0x0f];
}

std::string char_to_hex(unsigned char c)
{
	std::string hex;
	// First four bytes
	char left = (c >> 4);
	// Second four bytes
	char right = (c & 0x0f);

	hex += replace(left);
	hex += replace(right);
	return hex;
}

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

void PushNotification(const int& nSocketIndex, const int nFileEvent, const std::wstring& strFilePath)
{
	NOTIFY_FILE_ITEM* pThreadData = g_pThreadData[nSocketIndex];
	if ((pThreadData->hResourceMutex != nullptr) &&
		(pThreadData->hEmptySemaphore != nullptr) &&
		(pThreadData->hOccupiedSemaphore != nullptr))
	{
		WaitForSingleObject(pThreadData->hEmptySemaphore, INFINITE);
		WaitForSingleObject(pThreadData->hResourceMutex, INFINITE);

		TRACE(_T("[PushNotification] nFileEvent = %d, strFilePath = \"%s\"\n"), nFileEvent, strFilePath.c_str());
		pThreadData->arrNotifyData[pThreadData->nNextIn].nFileEvent = nFileEvent;
		pThreadData->arrNotifyData[pThreadData->nNextIn].strFilePath = strFilePath;
		pThreadData->nNextIn++;
		pThreadData->nNextIn %= NOTIFY_FILE_SIZE;

		ReleaseSemaphore(pThreadData->hResourceMutex, 1, nullptr);
		ReleaseSemaphore(pThreadData->hOccupiedSemaphore, 1, nullptr);
	}
}

void PopNotification(const int& nSocketIndex, int& nFileEvent, std::wstring& strFilePath)
{
	NOTIFY_FILE_ITEM* pThreadData = g_pThreadData[nSocketIndex];
	if ((pThreadData->hResourceMutex != nullptr) &&
		(pThreadData->hEmptySemaphore != nullptr) &&
		(pThreadData->hOccupiedSemaphore != nullptr))
	{
		WaitForSingleObject(pThreadData->hOccupiedSemaphore, INFINITE);
		WaitForSingleObject(pThreadData->hResourceMutex, INFINITE);

		nFileEvent = pThreadData->arrNotifyData[pThreadData->nNextOut].nFileEvent;
		strFilePath = pThreadData->arrNotifyData[pThreadData->nNextOut].strFilePath;
		pThreadData->nNextOut++;
		pThreadData->nNextOut %= NOTIFY_FILE_SIZE;
		TRACE(_T("[PopNotification] nFileEvent = %d, strFilePath = \"%s\"\n"), nFileEvent, strFilePath.c_str());

		ReleaseSemaphore(pThreadData->hResourceMutex, 1, nullptr);
		ReleaseSemaphore(pThreadData->hEmptySemaphore, 1, nullptr);
	}
}

DWORD WINAPI IntelliDiskThread(LPVOID lpParam)
{
	unsigned char pBuffer[MAX_BUFFER] = { 0, };
	int nLength = 0;
	const int nSocketIndex = *(int*)(lpParam);
	TRACE(_T("nSocketIndex = %d\n"), (nSocketIndex));
	CWSocket& pApplicationSocket = g_pClientSocket[nSocketIndex];
	ASSERT(pApplicationSocket.IsCreated());
	std::wstring strComputerID;

	g_pThreadData[nSocketIndex] = new NOTIFY_FILE_ITEM;
	ASSERT(g_pThreadData[nSocketIndex] != nullptr);
	NOTIFY_FILE_ITEM* pThreadData = g_pThreadData[nSocketIndex];
	ASSERT(pThreadData != nullptr);
	pThreadData->hOccupiedSemaphore = CreateSemaphore(nullptr, 0, NOTIFY_FILE_SIZE, nullptr);
	pThreadData->hEmptySemaphore = CreateSemaphore(nullptr, NOTIFY_FILE_SIZE, NOTIFY_FILE_SIZE, nullptr);
	pThreadData->hResourceMutex = CreateSemaphore(nullptr, 1, 1, nullptr);
	pThreadData->nNextIn = pThreadData->nNextOut = 0;

	g_bIsConnected[nSocketIndex] = false;

	while (g_bServerRunning)
	{
		try
		{
			if (!pApplicationSocket.IsCreated())
				break;

			if (pApplicationSocket.IsReadible(1000))
			{
				nLength = sizeof(pBuffer);
				ZeroMemory(pBuffer, sizeof(pBuffer));
				if (ReadBuffer(nSocketIndex, pApplicationSocket, pBuffer, nLength, true, false))
				{
					const std::string strCommand = (char*) &pBuffer[3];
					if (strCommand.compare("IntelliDisk") == 0)
					{
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
											VERIFY(UploadFile(nSocketIndex, pApplicationSocket, strFilePath));
											// Notify all other Clients
											for (int nThreadIndex = 0; nThreadIndex < g_nSocketCount; nThreadIndex++)
											{
												if (nThreadIndex != nSocketIndex) // skip current thread queue
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
												VERIFY(DeleteFile(nSocketIndex, pApplicationSocket, strFilePath));
												// Notify all other Clients
												for (int nThreadIndex = 0; nThreadIndex < g_nSocketCount; nThreadIndex++)
												{
													if (nThreadIndex != nSocketIndex) // skip current thread queue
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
				if (!g_bServerRunning)
				{
					const std::string strCommand = "Restart";
					nLength = (int)strCommand.length() + 1;
					if (WriteBuffer(nSocketIndex, pApplicationSocket, (unsigned char*)strCommand.c_str(), nLength, true, true))
					{
						TRACE(_T("Restart!\n"));
					}
				}
				else
				{
					if (pThreadData->nNextIn != pThreadData->nNextOut)
					{
						int nFileEvent = 0;
						std::wstring strFilePath;
						PopNotification(nSocketIndex, nFileEvent, strFilePath);
						if (ID_FILE_DOWNLOAD == nFileEvent)
						{
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
DWORD WINAPI CreateDatabase(LPVOID lpParam)
{
	UNREFERENCED_PARAMETER(lpParam);
	try
	{
		TRACE(_T("CreateDatabase()\n"));
		g_nServicePort = LoadServicePort();
		if (!LoadAppSettings(g_strHostName, g_nHostPort, g_strDatabase, g_strUsername, g_strPassword))
		{
			// return (DWORD)-1;
		}
		g_pServerSocket.CreateAndBind(g_nServicePort, SOCK_STREAM, AF_INET);
		if (g_pServerSocket.IsCreated())
		{
			g_bServerRunning = true;
			g_pServerSocket.Listen(MAX_SOCKET_CONNECTIONS);
			while (g_bServerRunning)
			{
				g_pServerSocket.Accept(g_pClientSocket[g_nSocketCount]);
				if (g_bServerRunning)
				{
					const int nSocketIndex = g_nSocketCount;
					g_hThreadArray[g_nThreadCount] = CreateThread(nullptr, 0, IntelliDiskThread, (int*)&nSocketIndex, 0, &m_dwThreadID[g_nThreadCount]);
					ASSERT(g_hThreadArray[g_nThreadCount] != nullptr);
					g_nSocketCount++;
					g_nThreadCount++;
				}
				else
				{
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

void StartProcessingThread()
{
	TRACE(_T("StartProcessingThread()\n"));
	g_hThreadArray[g_nThreadCount] = CreateThread(nullptr, 0, CreateDatabase, nullptr, 0, &m_dwThreadID[g_nThreadCount]);
	ASSERT(g_hThreadArray[g_nThreadCount] != nullptr);
	g_nThreadCount++;
}

void StopProcessingThread()
{
	CWSocket pClosingSocket;
	try
	{
		TRACE(_T("StopProcessingThread()\n"));
		if (g_bServerRunning)
		{
			g_bServerRunning = false;
			pClosingSocket.CreateAndConnect(IntelliDiskIP, g_nServicePort);
			WaitForMultipleObjects(g_nThreadCount, g_hThreadArray, TRUE, INFINITE);
			pClosingSocket.Close();
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
