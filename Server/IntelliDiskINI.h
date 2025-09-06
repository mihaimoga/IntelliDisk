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

#pragma once

#ifndef __INTELLIDISK_INI__
#define __INTELLIDISK_INI__

/**
 * @brief Default IP address for the IntelliDisk server.
 */
#define IntelliDiskIP _T("127.0.0.1")

 /**
  * @brief Default port for the IntelliDisk service.
  */
#define IntelliDiskPort 8080

  /**
   * @brief XML section name for IntelliDisk settings.
   */
#define IntelliDiskSection _T("IntelliDisk")

   /**
	* @brief Loads the service port from the IntelliDisk XML settings file.
	* @return The service port number, or the default IntelliDiskPort on error.
	*/
const int LoadServicePort();

/**
 * @brief Saves the service port to the IntelliDisk XML settings file.
 * @param nServicePort The service port number to save.
 * @return true on success, false on error.
 */
bool SaveServicePort(const int nServicePort);

/**
 * @brief Loads database and server connection settings from the IntelliDisk XML file.
 * @param strHostName [out] Host name for the database/server.
 * @param nHostPort [out] Port number.
 * @param strDatabase [out] Database name.
 * @param strUsername [out] Username for authentication.
 * @param strPassword [out] Password for authentication.
 * @return true on success, false on error.
 */
bool LoadAppSettings(std::wstring& strHostName, int& nHostPort, std::wstring& strDatabase, std::wstring& strUsername, std::wstring& strPassword);

/**
 * @brief Saves database and server connection settings to the IntelliDisk XML file.
 * @param strHostName Host name for the database/server.
 * @param nHostPort Port number.
 * @param strDatabase Database name.
 * @param strUsername Username for authentication.
 * @param strPassword Password for authentication.
 * @return true on success, false on error.
 */
bool SaveAppSettings(const std::wstring& strHostName, const int nHostPort, const std::wstring& strDatabase, const std::wstring& strUsername, const std::wstring& strPassword);

#endif
