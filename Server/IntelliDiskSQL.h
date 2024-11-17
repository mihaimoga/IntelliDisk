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

#ifndef __INTELLIDISK_SQL__
#define __INTELLIDISK_SQL__

#include "SocMFC.h"
#include "ODBCWrappers.h"

//Another flavour of an ODBC_CHECK_RETURN macro
#define ODBC_CHECK_RETURN_FALSE(nRet, handle) \
	handle.ValidateReturnValue(nRet); \
	if (!SQL_SUCCEEDED(nRet)) \
{ \
	return false; \
}

class CGenericStatement // execute one SQL statement; no output returned
{
public:
	// Methods
	bool Execute(CODBC::CConnection& pDbConnect, LPCTSTR lpszSQL)
	{
		// Create the statement object
		CODBC::CStatement statement;
		SQLRETURN nRet = statement.Create(pDbConnect);
		ODBC_CHECK_RETURN_FALSE(nRet, statement);

		// Prepare the statement
#pragma warning(suppress: 26465 26490 26492)
		nRet = statement.Prepare(const_cast<SQLTCHAR*>(reinterpret_cast<const SQLTCHAR*>(lpszSQL)));
		ODBC_CHECK_RETURN_FALSE(nRet, statement);

		// Execute the statement
		nRet = statement.Execute();
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		return true;
	}
};

bool DownloadFile(const int nSocketIndex, CWSocket& pApplicationSocket, const std::wstring& strFilePath);
bool UploadFile(const int nSocketIndex, CWSocket& pApplicationSocket, const std::wstring& strFilePath);
bool DeleteFile(const int nSocketIndex, CWSocket& pApplicationSocket, const std::wstring& strFilePath);

#endif
