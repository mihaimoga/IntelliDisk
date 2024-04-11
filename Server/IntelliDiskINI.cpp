/* Copyright (C) 2022-2024 Stefan-Mihai MOGA
This file is part of IntelliDisk application developed by Stefan-Mihai MOGA.

IntelliDisk is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Open
Source Initiative, either version 3 of the License, or any later version.

IntelliDisk is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
IntelliDisk. If not, see <http://www.opensource.org/licenses/gpl-3.0.html>*/

#include "pch.h"
#include "IntelliDiskINI.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

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
		const HRESULT hr{ CoInitialize(nullptr) };
		if (FAILED(hr))
			return false;

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
