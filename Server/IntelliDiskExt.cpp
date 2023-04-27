/* This file is part of IntelliDisk application developed by Stefan-Mihai MOGA.

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

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define IntelliDiskIP _T("127.0.0.1")
#define IntelliDiskPort 8080
#define IntelliDiskSection _T("IntelliDisk")

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

bool bThreadRunning = false;
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

std::wstring utf8_to_wstring(const std::string& str)
{
	// convert UTF-8 string to wstring
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	return myconv.from_bytes(str);
}

std::string wstring_to_utf8(const std::wstring& str)
{
	// convert wstring to UTF-8 string
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	return myconv.to_bytes(str);
}

static const std::string base64_chars =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

static inline bool is_base64(unsigned char c)
{
	return (isalnum(c) || (c == '+') || (c == '/'));
}

unsigned int EncodeBase64(const unsigned char* bytes_to_encode, unsigned int in_len, unsigned char* encoded_buffer, unsigned int& out_len)
{
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3] = { 0, 0, 0 };
	unsigned char char_array_4[4] = { 0, 0, 0, 0 };

	out_len = 0;
	while (in_len--)
	{
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3)
		{
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for (i = 0; i < 4; i++)
			{
				encoded_buffer[out_len++] = base64_chars[char_array_4[i]];
			}
			i = 0;
		}
	}

	if (i)
	{
		for (j = i; j < 3; j++)
		{
			char_array_3[j] = '\0';
		}

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; j < (i + 1); j++)
		{
			encoded_buffer[out_len++] = base64_chars[char_array_4[j]];
		}

		while (i++ < 3)
		{
			encoded_buffer[out_len++] = '=';
		}
	}

	return out_len;
}

unsigned int DecodeBase64(const unsigned char* encoded_string, unsigned int in_len, unsigned char* decoded_buffer, unsigned int& out_len)
{
	size_t i = 0;
	size_t j = 0;
	int in_ = 0;
	unsigned char char_array_3[3] = { 0, 0, 0 };
	unsigned char char_array_4[4] = { 0, 0, 0, 0 };

	out_len = 0;
	while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
	{
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i == 4)
		{
			for (i = 0; i < 4; i++)
			{
				char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));
			}

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; i < 3; i++)
			{
				decoded_buffer[out_len++] = char_array_3[i];
			}
			i = 0;
		}
	}

	if (i)
	{
		for (j = i; j < 4; j++)
		{
			char_array_4[j] = 0;
		}

		for (j = 0; j < 4; j++)
		{
			char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));
		}

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++)
		{
			decoded_buffer[out_len++] = char_array_3[j];
		}
	}
	return out_len;
}

const int MAX_BUFFER = 0x10000;
bool g_bIsConnected[MAX_SOCKET_CONNECTIONS];

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
				TRACE(_T("Buffer Received\n"));
				nReturn = (pBuffer[nLength - 1] == calcLRC(&pBuffer[3], (nLength - 5))) ? ACK : NAK;
			}
			TRACE(_T("%s Sent\n"), ((ACK == nReturn) ? _T("ACK") : _T("NAK")));
			VERIFY(pApplicationSocket.Send(&nReturn, sizeof(nReturn)) == 1);
		} while ((ACK != nReturn) && (++nCount < 3));
		if (ReceiveEOT)
		{
			nLength = MAX_BUFFER;
			if (pApplicationSocket.IsReadible(1000) &&
				((nLength = pApplicationSocket.Receive(pBuffer, nLength)) > 0) &&
				(EOT == pBuffer[nLength - 1]))
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
	return (ACK == nReturn);
}

bool WriteBuffer(const int nSocketIndex, CWSocket& pApplicationSocket, const unsigned char* pBuffer, const int nLength, const bool SendENQ, const bool SendEOT)
{
	int nCount = 0;
	unsigned char nReturn = ACK;
	unsigned char pPacket[MAX_BUFFER] = { 0, };

	try
	{
		ASSERT(nLength < sizeof(pPacket));
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
				VERIFY(pApplicationSocket.Receive(&nReturn, sizeof(nReturn)) == 1);
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
	return (nReturn == ACK);
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

	while (bThreadRunning)
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
										nLength = sizeof(pBuffer);
										ZeroMemory(pBuffer, sizeof(pBuffer));
										if (((nLength = pApplicationSocket.Receive(pBuffer, nLength)) > 0) &&
											(EOT == pBuffer[nLength - 1]))
										{
											TRACE(_T("EOT Received\n"));
										}
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
											nLength = sizeof(pBuffer);
											ZeroMemory(pBuffer, sizeof(pBuffer));
											if (((nLength = pApplicationSocket.Receive(pBuffer, nLength)) > 0) &&
												(EOT == pBuffer[nLength - 1]))
											{
												TRACE(_T("EOT Received\n"));
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
												nLength = sizeof(pBuffer);
												ZeroMemory(pBuffer, sizeof(pBuffer));
												if (((nLength = pApplicationSocket.Receive(pBuffer, nLength)) > 0) &&
													(EOT == pBuffer[nLength - 1]))
												{
													TRACE(_T("EOT Received\n"));
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
				if (!bThreadRunning)
				{
					const std::string strCommand = "Restart";
					nLength = (int)strCommand.length() + 1;
					if (WriteBuffer(nSocketIndex, pApplicationSocket, (unsigned char*)strCommand.c_str(), nLength, true, true))
					{
						TRACE(_T("Restart!\n"));
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
	g_nServicePort = LoadServicePort();
	if (!LoadAppSettings(g_strHostName, g_nHostPort, g_strDatabase, g_strUsername, g_strPassword))
	{
		// return (DWORD)-1;
	}
	try
	{
		TRACE(_T("CreateDatabase()\n"));
		g_pServerSocket.CreateAndBind(g_nServicePort, SOCK_STREAM, AF_INET);
		if (g_pServerSocket.IsCreated())
		{
			bThreadRunning = true;
			g_pServerSocket.Listen(MAX_SOCKET_CONNECTIONS);
			while (bThreadRunning)
			{
				g_pServerSocket.Accept(g_pClientSocket[g_nSocketCount]);
				if (bThreadRunning)
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
		if (bThreadRunning)
		{
			bThreadRunning = false;
			pClosingSocket.CreateAndConnect(IntelliDiskIP, g_nServicePort);
			WaitForMultipleObjects(g_nThreadCount, g_hThreadArray, TRUE, INFINITE);
			pClosingSocket.Close();
			// g_nSocketCount = 0;
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

const std::wstring GetAppSettingsFilePath()
{
	TCHAR lpszModuleFilePath[_MAX_PATH];
	TCHAR lpszDrive[_MAX_DRIVE];
	TCHAR lpszDirectory[_MAX_DIR];
	TCHAR lpszFilename[_MAX_FNAME];
	TCHAR lpszExtension[_MAX_EXT];
	TCHAR lpszFullPath[_MAX_PATH];

	WCHAR* lpszSpecialFolderPath = nullptr;
	if ((SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &lpszSpecialFolderPath)) == S_OK)
	{
		std::wstring result(lpszSpecialFolderPath);
		CoTaskMemFree(lpszSpecialFolderPath);
		result += _T("\\IntelliDisk.xml");
		return result;
	}

	VERIFY(0 != GetModuleFileName(NULL, lpszModuleFilePath, _MAX_PATH));
	VERIFY(0 == _tsplitpath_s(AfxGetApp()->m_pszHelpFilePath, lpszDrive, _MAX_DRIVE, lpszDirectory, _MAX_DIR, lpszFilename, _MAX_FNAME, lpszExtension, _MAX_EXT));
	VERIFY(0 == _tmakepath_s(lpszFullPath, _MAX_PATH, lpszDrive, lpszDirectory, _T("IntelliDisk"), _T(".xml")));
	return lpszFullPath;
}

#include "AppSettings.h"
const int LoadServicePort()
{
	int nServicePort = IntelliDiskPort;
	TRACE(_T("LoadServicePort\n"));
	try {
		const HRESULT hr{ CoInitialize(nullptr) };
		if (FAILED(hr))
			return nServicePort;

		CXMLAppSettings pAppSettings(GetAppSettingsFilePath(), true, true);
		nServicePort = pAppSettings.GetInt(IntelliDiskSection, _T("ServicePort"));
	}
	catch (CAppSettingsException& pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException.GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
	}
	return nServicePort;
}

bool SaveServicePort(const int nServicePort)
{
	TRACE(_T("SaveServicePort\n"));
	try {
		CXMLAppSettings pAppSettings(GetAppSettingsFilePath(), true, true);
		pAppSettings.WriteInt(IntelliDiskSection, _T("ServicePort"), nServicePort);
	}
	catch (CAppSettingsException& pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException.GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		return false;
	}
	return true;
}

bool LoadAppSettings(std::wstring& strHostName, int& nHostPort, std::wstring& strDatabase, std::wstring& strUsername, std::wstring& strPassword)
{
	// LoadServicePort() must be called before to do CoInitialize(nullptr)
	TRACE(_T("LoadAppSettings\n"));
	try {
		CXMLAppSettings pAppSettings(GetAppSettingsFilePath(), true, true);
		strHostName = pAppSettings.GetString(IntelliDiskSection, _T("HostName"));
		nHostPort = pAppSettings.GetInt(IntelliDiskSection, _T("HostPort"));
		strDatabase = pAppSettings.GetString(IntelliDiskSection, _T("Database"));
		strUsername = pAppSettings.GetString(IntelliDiskSection, _T("Username"));
		strPassword = pAppSettings.GetString(IntelliDiskSection, _T("Password"));
	}
	catch (CAppSettingsException& pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException.GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		return false;
	}
	return true;
}

bool SaveAppSettings(const std::wstring& strHostName, const int nHostPort, const std::wstring& strDatabase, const std::wstring& strUsername, const std::wstring& strPassword)
{
	// LoadServicePort() must be called before to do CoInitialize(nullptr)
	TRACE(_T("SaveAppSettings\n"));
	try {
		CXMLAppSettings pAppSettings(GetAppSettingsFilePath(), true, true);
		pAppSettings.WriteString(IntelliDiskSection, _T("HostName"), strHostName.c_str());
		pAppSettings.WriteInt(IntelliDiskSection, _T("HostPort"), nHostPort);
		pAppSettings.WriteString(IntelliDiskSection, _T("Database"), strDatabase.c_str());
		pAppSettings.WriteString(IntelliDiskSection, _T("Username"), strUsername.c_str());
		pAppSettings.WriteString(IntelliDiskSection, _T("Password"), strPassword.c_str());
	}
	catch (CAppSettingsException& pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException.GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		return false;
	}
	return true;
}