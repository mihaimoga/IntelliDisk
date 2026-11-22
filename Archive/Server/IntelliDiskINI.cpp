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
#include "IntelliDiskINI.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/**
 * @brief Retrieves the full path of the current module (executable or DLL)
 * @param pdwLastError Optional pointer to receive the last error code
 * @return The full path as a CString, or an empty string on failure
 * 
 * BUFFER RESIZING STRATEGY:
 * =========================
 * Windows doesn't tell us the required buffer size upfront, so we use
 * a doubling strategy: start with _MAX_PATH and double until successful.
 * This handles paths longer than MAX_PATH (260 chars) on Windows 10+.
 */
CString GetModuleFileName(_Inout_opt_ DWORD* pdwLastError = nullptr)
{
	CString strModuleFileName;
	DWORD dwSize{ _MAX_PATH };  // Start with typical path length
	while (true)
	{
		// Allocate buffer of current size
		TCHAR* pszModuleFileName{ strModuleFileName.GetBuffer(dwSize) };
		const DWORD dwResult{ ::GetModuleFileName(nullptr, pszModuleFileName, dwSize) };

		if (dwResult == 0)
		{
			// Function failed - return error
			if (pdwLastError != nullptr)
				*pdwLastError = GetLastError();
			strModuleFileName.ReleaseBuffer(0);
			return CString{};
		}
		else if (dwResult < dwSize)
		{
			// Success - buffer was large enough
			if (pdwLastError != nullptr)
				*pdwLastError = ERROR_SUCCESS;
			strModuleFileName.ReleaseBuffer(dwResult);
			return strModuleFileName;
		}
		else if (dwResult == dwSize)
		{
			// Buffer too small - double size and retry
			// When buffer is too small, GetModuleFileName returns dwSize
			strModuleFileName.ReleaseBuffer(0);
			dwSize *= 2;  // Double buffer size for next attempt
		}
	}
}

/**
 * @brief Gets the full path to the IntelliDisk XML settings file
 * @details Tries to use the user's profile directory, falls back to the executable's directory
 * @return The full path as a std::wstring
 * 
 * SETTINGS FILE LOCATION PRIORITY:
 * =================================
 * 1st choice: %USERPROFILE%\IntelliDisk.xml (user-specific settings)
 * 2nd choice: <EXE_DIR>\IntelliDisk.xml (fallback if profile unavailable)
 */
const std::wstring GetAppSettingsFilePath()
{
	// Try to get user's profile directory (e.g., C:\Users\Username)
	WCHAR* lpszSpecialFolderPath = nullptr;
	if ((SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &lpszSpecialFolderPath)) == S_OK)
	{
		// Success - use profile directory
		std::wstring result(lpszSpecialFolderPath);
		CoTaskMemFree(lpszSpecialFolderPath);  // Must free memory allocated by shell
		result += _T("\\IntelliDisk.xml");
		return result;
	}

	// Fallback: Profile directory unavailable, use executable's directory
	// This handles scenarios like running as a service without user profile
	CString strFilePath{ GetModuleFileName() };
	std::filesystem::path strFullPath{ strFilePath.GetString() };
	strFullPath.replace_filename(_T("IntelliDisk"));  // Replace exe name with "IntelliDisk"
	strFullPath.replace_extension(_T(".xml"));  // Set extension to .xml
	OutputDebugString(strFullPath.c_str());  // Log fallback path for debugging
	return strFullPath.c_str();
}

#include "AppSettings.h"

/**
 * @brief Loads the service port from the IntelliDisk XML settings file
 * @return The service port number, or the default IntelliDiskPort on error
 * 
 * XML STRUCTURE:
 * ==============
 * <Settings>
 *   <IntelliDisk>
 *     <ServicePort>8080</ServicePort>
 *   </IntelliDisk>
 * </Settings>
 */
const int LoadServicePort()
{
	int nServicePort = IntelliDiskPort;  // Default fallback value
	TRACE(_T("LoadServicePort\n"));
	try {
		// Initialize COM for XML parsing (required by CXMLAppSettings)
		const HRESULT hr{ CoInitialize(nullptr) };
		if (FAILED(hr))
			return nServicePort;  // COM initialization failed, use default

		// Open XML settings file (create if not exists, read/write mode)
		CXMLAppSettings pAppSettings(GetAppSettingsFilePath(), true, true);
		// Read service port from [IntelliDisk] section
		nServicePort = pAppSettings.GetInt(IntelliDiskSection, _T("ServicePort"));
	}
	catch (CAppSettingsException& pException)
	{
		// XML parsing error or file not found - log and return default
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException.GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
	}
	return nServicePort;
}

/**
 * @brief Saves the service port to the IntelliDisk XML settings file
 * @param nServicePort The service port number to save
 * @return true on success, false on error
 */
bool SaveServicePort(const int nServicePort)
{
	TRACE(_T("SaveServicePort\n"));
	try {
		// Open or create XML settings file
		CXMLAppSettings pAppSettings(GetAppSettingsFilePath(), true, true);
		// Write service port to [IntelliDisk] section
		pAppSettings.WriteInt(IntelliDiskSection, _T("ServicePort"), nServicePort);
	}
	catch (CAppSettingsException& pException)
	{
		// File write error or XML formatting error
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException.GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		return false;
	}
	return true;
}

/**
 * @brief Loads database and server connection settings from the IntelliDisk XML file
 * @param strHostName [out] Host name for the database/server
 * @param nHostPort [out] Port number
 * @param strDatabase [out] Database name
 * @param strUsername [out] Username for authentication
 * @param strPassword [out] Password for authentication
 * @return true on success, false on error
 * 
 * XML STRUCTURE:
 * ==============
 * <Settings>
 *   <IntelliDisk>
 *     <HostName>localhost</HostName>
 *     <HostPort>3306</HostPort>
 *     <Database>intellidisk</Database>
 *     <Username>root</Username>
 *     <Password>encrypted_password</Password>
 *   </IntelliDisk>
 * </Settings>
 */
bool LoadAppSettings(std::wstring& strHostName, int& nHostPort, std::wstring& strDatabase, std::wstring& strUsername, std::wstring& strPassword)
{
	// Note: LoadServicePort() should be called first to do CoInitialize(nullptr)
	// If this is called first, it will initialize COM for this thread
	TRACE(_T("LoadAppSettings\n"));
	try {
		// Initialize COM for XML parsing
		const HRESULT hr{ CoInitialize(nullptr) };
		if (FAILED(hr))
			return false;

		// Open XML settings file
		CXMLAppSettings pAppSettings(GetAppSettingsFilePath(), true, true);
		// Read all database connection settings from [IntelliDisk] section
		strHostName = pAppSettings.GetString(IntelliDiskSection, _T("HostName"));
		nHostPort = pAppSettings.GetInt(IntelliDiskSection, _T("HostPort"));
		strDatabase = pAppSettings.GetString(IntelliDiskSection, _T("Database"));
		strUsername = pAppSettings.GetString(IntelliDiskSection, _T("Username"));
		strPassword = pAppSettings.GetString(IntelliDiskSection, _T("Password"));
	}
	catch (CAppSettingsException& pException)
	{
		// Missing required settings or XML parse error
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException.GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		return false;
	}
	return true;
}

/**
 * @brief Saves database and server connection settings to the IntelliDisk XML file
 * @param strHostName Host name for the database/server
 * @param nHostPort Port number
 * @param strDatabase Database name
 * @param strUsername Username for authentication
 * @param strPassword Password for authentication
 * @return true on success, false on error
 */
bool SaveAppSettings(const std::wstring& strHostName, const int nHostPort, const std::wstring& strDatabase, const std::wstring& strUsername, const std::wstring& strPassword)
{
	// Note: LoadServicePort() should be called first to do CoInitialize(nullptr)
	TRACE(_T("SaveAppSettings\n"));
	try {
		// Open or create XML settings file
		CXMLAppSettings pAppSettings(GetAppSettingsFilePath(), true, true);
		// Write all database connection settings to [IntelliDisk] section
		// These settings persist across service restarts
		pAppSettings.WriteString(IntelliDiskSection, _T("HostName"), strHostName.c_str());
		pAppSettings.WriteInt(IntelliDiskSection, _T("HostPort"), nHostPort);
		pAppSettings.WriteString(IntelliDiskSection, _T("Database"), strDatabase.c_str());
		pAppSettings.WriteString(IntelliDiskSection, _T("Username"), strUsername.c_str());
		pAppSettings.WriteString(IntelliDiskSection, _T("Password"), strPassword.c_str());
	}
	catch (CAppSettingsException& pException)
	{
		// File write error, permission denied, or disk full
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException.GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		return false;
	}
	return true;
}
