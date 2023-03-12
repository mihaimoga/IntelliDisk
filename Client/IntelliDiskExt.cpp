/* This file is part of IntelliDisk application developed by Mihai MOGA.

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

#define SECURITY_WIN32
#include "Security.h"
#pragma comment(lib, "Secur32.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

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

const std::string GetMachineID()
{
	/*	LINUX:
	#include <unistd.h>
	#include <limits.h>

	char hostname[HOST_NAME_MAX];
	char username[LOGIN_NAME_MAX];
	gethostname(hostname, HOST_NAME_MAX);
	getlogin_r(username, LOGIN_NAME_MAX);
	*/

DWORD nLength = 0x1000;
TCHAR lpszUserName[0x1000] = { 0, };
if (GetUserNameEx(NameUserPrincipal, lpszUserName, &nLength))
{
	lpszUserName[nLength] = 0;
	TRACE(_T("UserName = %s\n"), lpszUserName);
}
else
{
	nLength = 0x1000;
	if (GetUserName(lpszUserName, &nLength) != 0)
	{
		lpszUserName[nLength] = 0;
		TRACE(_T("UserName = %s\n"), lpszUserName);
	}
}

nLength = 0x1000;
TCHAR lpszComputerName[0x1000] = { 0, };
if (GetComputerNameEx(ComputerNamePhysicalDnsFullyQualified, lpszComputerName, &nLength))
{
	lpszComputerName[nLength] = 0;
	TRACE(_T("ComputerName = %s\n"), lpszComputerName);
}
else
{
	nLength = 0x1000;
	if (GetComputerName(lpszComputerName, &nLength) != 0)
	{
		lpszComputerName[nLength] = 0;
		TRACE(_T("ComputerName = %s\n"), lpszComputerName);
	}
}

std::wstring result(lpszUserName);
result += _T(":");
result += lpszComputerName;
return wstring_to_utf8(result);
}

const std::wstring GetSpecialFolder()
{
	WCHAR* lpszSpecialFolderPath = NULL;
	if ((SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &lpszSpecialFolderPath)) == S_OK)
	{
		std::wstring result(lpszSpecialFolderPath);
		CoTaskMemFree(lpszSpecialFolderPath);
		result += _T("\\IntelliDisk\\");
		return result;
	}
	return _T("");
}

bool InstallStartupApps(bool bInstallStartupApps)
{
	HKEY regValue;
	bool result = false;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_ALL_ACCESS, &regValue) == ERROR_SUCCESS)
	{
		if (bInstallStartupApps) // Add program to Starup Apps
		{
			TCHAR lpszApplicationBuffer[MAX_PATH + 1] = { 0, };
			if (GetModuleFileName(NULL, lpszApplicationBuffer, MAX_PATH) > 0)
			{
				std::wstring quoted(_T("\""));
				quoted += lpszApplicationBuffer;
				quoted += _T("\"");
				const size_t length = quoted.length() * sizeof(TCHAR);
				if (RegSetValueEx(regValue, _T("IntelliDisk"), 0, REG_SZ, (LPBYTE)quoted.c_str(), (DWORD)length) == ERROR_SUCCESS)
				{
					result = true;
				}
			}
		}
		else // Remove program from Startup Apps
		{
			if (RegDeleteValue(regValue, _T("IntelliDisk")) == ERROR_SUCCESS)
			{
				result = true;
			}
		}
		RegCloseKey(regValue);
	}
	return result;
}

UINT DirCallback(CFileInformation fiObject, EFileAction faAction, LPVOID lpData)
{
	const int nFileEvent = (IS_DELETE_FILE(faAction)) ? ID_FILE_DELETE : ID_FILE_UPLOAD;
	const std::wstring strFilePath = fiObject.GetFilePath();
	AddNewItem(nFileEvent, strFilePath, lpData);

	return 0; // success
}

const int MAX_BUFFER = 0x10000;

bool WriteBuffer(CWSocket& pApplicationSocket, const char* pBuffer, int nLength)
{
	int nCount = 0;
	char nReturn = ACK;
	char pPacket[MAX_BUFFER] = { 0, };

	try
	{
		ASSERT(nLength < sizeof(pPacket));
		ZeroMemory(pPacket, sizeof(pPacket));
		CopyMemory(&pPacket[3], pBuffer, nLength);
		pPacket[0] = STX;
		pPacket[1] = nLength / 0x100;
		pPacket[2] = nLength % 0x100;
		pPacket[3 + nLength] = ETX;
		pPacket[4 + nLength] = calcLRC((byte*) pBuffer, nLength);
		do {
			if (pApplicationSocket.Send(pPacket, (5 + nLength)) == (5 + nLength))
			{
				VERIFY(pApplicationSocket.Receive(&nReturn, sizeof(nReturn)) == 1);
			}
		} while ((nReturn != ACK) && (++nCount <= 3));
	}
	catch (CWSocketException* pException)
	{
		TCHAR lpszErrorMessage[0x100] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, sizeof(lpszErrorMessage));
		TRACE(_T("%s\n"), lpszErrorMessage);
		delete pException;
		pException = NULL;
		return false;
	}
	return (nReturn == ACK);
}

int g_nPingCount = 0;
bool g_bThreadRunning = true;
bool g_bIsConnected = false;
DWORD WINAPI ProducerThread(LPVOID lpParam)
{
	char pBuffer[MAX_BUFFER] = { 0, };
	int nLength = 0;

	CMainFrame* pMainFrame = (CMainFrame*)lpParam;
	CWSocket& pApplicationSocket = pMainFrame->m_pApplicationSocket;
	HANDLE& hSocketMutex = pMainFrame->m_hSocketMutex;

	while (g_bThreadRunning)
	{
		try
		{
			WaitForSingleObject(hSocketMutex, INFINITE);

			if (!pApplicationSocket.IsCreated())
			{
				pApplicationSocket.CreateAndConnect(pMainFrame->m_strServerIP, pMainFrame->m_nServerPort);
				const std::string strCommand = "IntelliDisk";
				nLength = (int)strCommand.length() + 1;
				if (WriteBuffer(pApplicationSocket, strCommand.c_str(), nLength))
				{
					TRACE(_T("Client connected!\n"));
					const std::string strMachineID = GetMachineID();
					int nComputerLength = (int)strMachineID.length() + 1;
					if (WriteBuffer(pApplicationSocket, strMachineID.c_str(), nComputerLength))
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
					g_nPingCount = 0;
				}
				else
				{
					if (60 == ++g_nPingCount) // every 60 seconds
					{
						g_nPingCount = 0;
						const std::string strCommand = "Ping";
						nLength = (int)strCommand.length() + 1;
						if (WriteBuffer(pApplicationSocket, strCommand.c_str(), nLength))
						{
							TRACE(_T("Ping!\n"));
						}
					}
				}
			}

			ReleaseSemaphore(hSocketMutex, 1, NULL);
			Sleep(100);
		}
		catch (CWSocketException* pException)
		{
			TCHAR lpszErrorMessage[0x100] = { 0, };
			pException->GetErrorMessage(lpszErrorMessage, sizeof(lpszErrorMessage));
			TRACE(_T("%s\n"), lpszErrorMessage);
			delete pException;
			pException = NULL;
			g_bIsConnected = false;
			ReleaseSemaphore(hSocketMutex, 1, NULL);
			Sleep(60000);
			continue;
		}
	}
	pApplicationSocket.Close();
	TRACE(_T("exiting...\n"));
	return 0;
}

DWORD WINAPI ConsumerThread(LPVOID lpParam)
{
	char pBuffer[MAX_BUFFER] = { 0, };
	int nLength = 0;

	CMainFrame* pMainFrame = (CMainFrame*)lpParam;
	HANDLE& hOccupiedSemaphore = pMainFrame->m_hOccupiedSemaphore;
	HANDLE& hEmptySemaphore = pMainFrame->m_hEmptySemaphore;
	HANDLE& hResourceMutex = pMainFrame->m_hResourceMutex;
	HANDLE& hSocketMutex = pMainFrame->m_hSocketMutex;

	CWSocket& pApplicationSocket = pMainFrame->m_pApplicationSocket;

	while (g_bThreadRunning)
	{
		bool bSuccessfulOperation = true;

		WaitForSingleObject(hOccupiedSemaphore, INFINITE);
		WaitForSingleObject(hResourceMutex, INFINITE);

		const int nFileEvent = pMainFrame->m_pResourceArray[pMainFrame->m_nNextOut].nFileEvent;
		const std::wstring& strFilePath = pMainFrame->m_pResourceArray[pMainFrame->m_nNextOut].strFilePath;
		TRACE(_T("[ConsumerThread] nFileEvent = %d, strFilePath = \"%s\"\n"), nFileEvent, strFilePath.c_str());
		pMainFrame->m_nNextOut++;
		pMainFrame->m_nNextOut %= BSIZE;

		if (ID_STOP_PROCESS == nFileEvent)
		{
			TRACE(_T("Stopping...\n"));
			g_bThreadRunning = false;
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

		ReleaseSemaphore(hResourceMutex, 1, NULL);
		ReleaseSemaphore(hEmptySemaphore, 1, NULL);

		while (g_bThreadRunning && !g_bIsConnected);

		WaitForSingleObject(hSocketMutex, INFINITE);

		try
		{
			if (pApplicationSocket.IsWritable(1000))
			{
				if (ID_STOP_PROCESS == nFileEvent)
				{
					const std::string strCommand = "Close";
					nLength = (int)strCommand.length() + 1;
					if (WriteBuffer(pApplicationSocket, strCommand.c_str(), nLength))
					{
						TRACE("Closing...\n");
					}
				}
				else
				{
					if (ID_FILE_DOWNLOAD == nFileEvent)
					{
						const std::string strCommand = "Download";
						nLength = (int)strCommand.length() + 1;
						if (WriteBuffer(pApplicationSocket, strCommand.c_str(), nLength))
						{
							const std::string strASCII = wstring_to_utf8(strFilePath);
							const int nFileNameLength = (int)strASCII.length() + 1;
							if (WriteBuffer(pApplicationSocket, strASCII.c_str(), nFileNameLength))
							{
								TRACE(_T("Downloading %s...\n"), strFilePath.c_str());
							}
						}
					}
					else
					{
						if (ID_FILE_UPLOAD == nFileEvent)
						{
							const std::string strCommand = "Upload";
							nLength = (int)strCommand.length() + 1;
							if (WriteBuffer(pApplicationSocket, strCommand.c_str(), nLength))
							{
								const std::string strASCII = wstring_to_utf8(strFilePath);
								const int nFileNameLength = (int)strASCII.length() + 1;
								if (WriteBuffer(pApplicationSocket, strASCII.c_str(), nFileNameLength))
								{
									TRACE(_T("Uploading %s...\n"), strFilePath.c_str());
								}
							}
						}
						else
						{
							if (ID_FILE_DELETE == nFileEvent)
							{
								const std::string strCommand = "Delete";
								nLength = (int)strCommand.length() + 1;
								if (WriteBuffer(pApplicationSocket, strCommand.c_str(), nLength))
								{
									const std::string strASCII = wstring_to_utf8(strFilePath);
									const int nFileNameLength = (int)strASCII.length() + 1;
									if (WriteBuffer(pApplicationSocket, strASCII.c_str(), nFileNameLength))
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
			TCHAR lpszErrorMessage[0x100] = { 0, };
			pException->GetErrorMessage(lpszErrorMessage, sizeof(lpszErrorMessage));
			TRACE(_T("%s\n"), lpszErrorMessage);
			delete pException;
			pException = NULL;
			bSuccessfulOperation = false;
		}

		ReleaseSemaphore(hSocketMutex, 1, NULL);

		if (!bSuccessfulOperation)
		{
			TRACE(_T("Handle error!\n"));
			AddNewItem(nFileEvent, strFilePath, lpParam);
		}
	}
	return 0;
}

void AddNewItem(const int nFileEvent, const std::wstring& strFilePath, LPVOID lpParam)
{
	CMainFrame* pMainFrame = (CMainFrame*)lpParam;
	HANDLE& hOccupiedSemaphore = pMainFrame->m_hOccupiedSemaphore;
	HANDLE& hEmptySemaphore = pMainFrame->m_hEmptySemaphore;
	HANDLE& hResourceMutex = pMainFrame->m_hResourceMutex;

	WaitForSingleObject(hEmptySemaphore, INFINITE);
	WaitForSingleObject(hResourceMutex, INFINITE);

	TRACE(_T("[AddNewItem] nFileEvent = %d, strFilePath = \"%s\"\n"), nFileEvent, strFilePath.c_str());
	pMainFrame->m_pResourceArray[pMainFrame->m_nNextIn].nFileEvent = nFileEvent;
	pMainFrame->m_pResourceArray[pMainFrame->m_nNextIn].strFilePath = strFilePath;
	pMainFrame->m_nNextIn++;
	pMainFrame->m_nNextIn %= BSIZE;

	ReleaseSemaphore(hResourceMutex, 1, NULL);
	ReleaseSemaphore(hOccupiedSemaphore, 1, NULL);
}
