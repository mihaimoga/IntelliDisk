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

DWORD WINAPI ProducerThread(LPVOID lpParam)
{
	Sleep(10000);
	return 0;
}

DWORD WINAPI ConsumerThread(LPVOID lpParam)
{
	CMainFrame* pMainFrame = (CMainFrame*)lpParam;
	HANDLE& hOccupiedSemaphore = pMainFrame->m_hOccupiedSemaphore;
	HANDLE& hEmptySemaphore = pMainFrame->m_hEmptySemaphore;
	HANDLE& hResourceMutex = pMainFrame->m_hResourceMutex;

	bool bThreadRunning = true;
	while (bThreadRunning)
	{
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
			bThreadRunning = false;
		}
		else if (ID_FILE_DOWNLOAD == nFileEvent)
		{
			CString strMessage;
			strMessage.Format(_T("Uploading %s..."), strFilePath.c_str());
			pMainFrame->ShowMessage(strMessage.GetBuffer(), strFilePath);
			strMessage.ReleaseBuffer();
		}
		else if (ID_FILE_UPLOAD == nFileEvent)
		{
			CString strMessage;
			strMessage.Format(_T("Downloading %s..."), strFilePath.c_str());
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
