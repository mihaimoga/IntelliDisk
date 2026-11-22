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

#pragma once

#ifndef __INTELLIDISK_SQL__
#define __INTELLIDISK_SQL__

#include "SocMFC.h"
#include "ODBCWrappers.h"

/**
 * @brief Macro for ODBC error checking. Validates the return value of an ODBC call and returns false if the call failed.
 * @param nRet The return value from the ODBC function.
 * @param handle The ODBC handle (statement, connection, etc.) to validate.
 */
#define ODBC_CHECK_RETURN_FALSE(nRet, handle) \
	handle.ValidateReturnValue(nRet); \
	if (!SQL_SUCCEEDED(nRet)) \
{ \
	return false; \
}

/**
 * @brief Executes a generic SQL statement (no output expected).
 *        Used for simple SQL commands such as SET, DELETE, etc.
 */
class CGenericStatement
{
public:
	/**
	 * @brief Executes the given SQL statement on the provided ODBC connection.
	 * @param pDbConnect The ODBC connection.
	 * @param lpszSQL The SQL statement to execute.
	 * @return true on success, false on failure.
	 */
	bool Execute(CODBC::CConnection& pDbConnect, LPCTSTR lpszSQL)
	{
		CODBC::CStatement statement;
		SQLRETURN nRet = statement.Create(pDbConnect);
		ODBC_CHECK_RETURN_FALSE(nRet, statement);

#pragma warning(suppress: 26465 26490 26492)
		nRet = statement.Prepare(const_cast<SQLTCHAR*>(reinterpret_cast<const SQLTCHAR*>(lpszSQL)));
		ODBC_CHECK_RETURN_FALSE(nRet, statement);

		nRet = statement.Execute();
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		return true;
	}
};

/**
 * @brief Handles the download of a file from the server to a client.
 *        Streams file data from the database to the client socket, with SHA256 integrity check.
 * @param nSocketIndex Index of the client socket.
 * @param pApplicationSocket The socket to write to.
 * @param strFilePath The file path to download.
 * @return true on success, false on failure.
 */
bool DownloadFile(const int nSocketIndex, CWSocket& pApplicationSocket, const std::wstring& strFilePath);

/**
 * @brief Handles the upload of a file from a client to the server.
 *        Receives file data from the client socket and stores it in the database, with SHA256 integrity check.
 * @param nSocketIndex Index of the client socket.
 * @param pApplicationSocket The socket to read from.
 * @param strFilePath The file path to upload.
 * @return true on success, false on failure.
 */
bool UploadFile(const int nSocketIndex, CWSocket& pApplicationSocket, const std::wstring& strFilePath);

/**
 * @brief Handles the deletion of a file from the server database.
 *        Removes file data and metadata for the given file path.
 * @param nSocketIndex Index of the client socket.
 * @param pApplicationSocket The socket to read EOT from.
 * @param strFilePath The file path to delete.
 * @return true on success, false on failure.
 */
bool DeleteFile(const int nSocketIndex, CWSocket& pApplicationSocket, const std::wstring& strFilePath);

#endif
