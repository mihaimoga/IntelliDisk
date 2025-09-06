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
#include "SHA256.h"
#include "base64.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/**
 * @brief ODBC accessor for inserting a row into the `filename` table.
 */
class CFilenameInsertAccessor
{
public:
	TCHAR m_lpszFilepath[4000];
	__int64 m_nFilesize;

	BEGIN_ODBC_PARAM_MAP(CFilenameInsertAccessor)
		SET_ODBC_PARAM_TYPE(SQL_PARAM_INPUT)
		ODBC_PARAM_ENTRY(1, m_lpszFilepath)
		ODBC_PARAM_ENTRY(2, m_nFilesize)
	END_ODBC_PARAM_MAP()

	DEFINE_ODBC_COMMAND(CFilenameInsertAccessor, _T("INSERT INTO `filename` (`filepath`, `filesize`) VALUES (?, ?);"))

		void ClearRecord() noexcept { memset(this, 0, sizeof(*this)); }
};

/**
 * @brief Executes an INSERT for the `filename` table.
 */
class CFilenameInsert : public CODBC::CAccessor<CFilenameInsertAccessor>
{
public:
	bool Execute(CODBC::CConnection& pDbConnect, const std::wstring& lpszFilepath, const __int64& nFilesize)
	{
		ClearRecord();
		CODBC::CStatement statement;
		SQLRETURN nRet = statement.Create(pDbConnect);
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		nRet = statement.Prepare(GetDefaultCommand());
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
#pragma warning(suppress: 26485)
		_tcscpy_s(m_lpszFilepath, _countof(m_lpszFilepath), lpszFilepath.c_str());
		m_nFilesize = nFilesize;
		nRet = BindParameters(statement);
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		nRet = statement.Execute();
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		return true;
	}
};

/**
 * @brief ODBC accessor for selecting a row from the `filename` table.
 */
class CFilenameSelectAccessor
{
public:
	TCHAR m_lpszFilepath[4000];

	BEGIN_ODBC_PARAM_MAP(CFilenameSelectAccessor)
		SET_ODBC_PARAM_TYPE(SQL_PARAM_INPUT)
		ODBC_PARAM_ENTRY(1, m_lpszFilepath)
	END_ODBC_PARAM_MAP()

	DEFINE_ODBC_COMMAND(CFilenameSelectAccessor, _T("SET @last_filename_id = (SELECT `filename_id` FROM `filename` WHERE `filepath` = ?);"))

		void ClearRecord() noexcept { memset(this, 0, sizeof(*this)); }
};

/**
 * @brief Executes a SELECT for the `filename` table to set @last_filename_id.
 */
class CFilenameSelect : public CODBC::CAccessor<CFilenameSelectAccessor>
{
public:
	bool Execute(CODBC::CConnection& pDbConnect, const std::wstring& lpszFilepath)
	{
		ClearRecord();
		CODBC::CStatement statement;
		SQLRETURN nRet = statement.Create(pDbConnect);
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		nRet = statement.Prepare(GetDefaultCommand());
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
#pragma warning(suppress: 26485)
		_tcscpy_s(m_lpszFilepath, _countof(m_lpszFilepath), lpszFilepath.c_str());
		nRet = BindParameters(statement);
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		nRet = statement.Execute();
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		return true;
	}
};

/**
 * @brief ODBC accessor for updating a row in the `filename` table.
 */
class CFilenameUpdateAccessor
{
public:
	__int64 m_nFilesize;

	BEGIN_ODBC_PARAM_MAP(CFilenameUpdateAccessor)
		SET_ODBC_PARAM_TYPE(SQL_PARAM_INPUT)
		ODBC_PARAM_ENTRY(1, m_nFilesize)
	END_ODBC_PARAM_MAP()

	DEFINE_ODBC_COMMAND(CFilenameUpdateAccessor, _T("UPDATE `filename` SET `filesize` = ? WHERE `filename_id` = @last_filename_id;"))

		void ClearRecord() noexcept { memset(this, 0, sizeof(*this)); }
};

/**
 * @brief Executes an UPDATE for the `filename` table.
 */
class CFilenameUpdate : public CODBC::CAccessor<CFilenameUpdateAccessor>
{
public:
	bool Execute(CODBC::CConnection& pDbConnect, const __int64& nFilesize)
	{
		ClearRecord();
		CODBC::CStatement statement;
		SQLRETURN nRet = statement.Create(pDbConnect);
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		nRet = statement.Prepare(GetDefaultCommand());
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
#pragma warning(suppress: 26485)
		m_nFilesize = nFilesize;
		nRet = BindParameters(statement);
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		nRet = statement.Execute();
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		return true;
	}
};

/**
 * @brief ODBC accessor for inserting a row into the `filedata` table.
 */
class CFiledataInsertAccessor
{
public:
	TCHAR m_lpszContent[0x20000];
	__int64 m_nBase64;

	BEGIN_ODBC_PARAM_MAP(CFiledataInsertAccessor)
		SET_ODBC_PARAM_TYPE(SQL_PARAM_INPUT)
		ODBC_PARAM_ENTRY(1, m_lpszContent)
		ODBC_PARAM_ENTRY(2, m_nBase64)
	END_ODBC_PARAM_MAP()

	DEFINE_ODBC_COMMAND(CFiledataInsertAccessor, _T("INSERT INTO `filedata` (`filename_id`, `content`, `base64`) VALUES (@last_filename_id, ?, ?);"))

		void ClearRecord() noexcept { memset(this, 0, sizeof(*this)); }
};

/**
 * @brief Executes an INSERT for the `filedata` table.
 */
class CFiledataInsert : public CODBC::CAccessor<CFiledataInsertAccessor>
{
public:
	bool Execute(CODBC::CConnection& pDbConnect, const std::wstring& lpszContent, const __int64& nBase64)
	{
		ClearRecord();
		CODBC::CStatement statement;
		SQLRETURN nRet = statement.Create(pDbConnect);
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		nRet = statement.Prepare(GetDefaultCommand());
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
#pragma warning(suppress: 26485)
		_tcscpy_s(m_lpszContent, _countof(m_lpszContent), lpszContent.c_str());
		m_nBase64 = nBase64;
		nRet = BindParameters(statement);
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		nRet = statement.Execute();
		ODBC_CHECK_RETURN_FALSE(nRet, statement);
		return true;
	}
};

/**
 * @brief ODBC accessor for selecting the file size from the `filename` table.
 */
class CFilesizeSelectAccessor
{
public:
	__int64 m_nFilesize;

	BEGIN_ODBC_PARAM_MAP(CFilesizeSelectAccessor)
	END_ODBC_PARAM_MAP()

	BEGIN_ODBC_COLUMN_MAP(CFilesizeSelectAccessor)
		ODBC_COLUMN_ENTRY(1, m_nFilesize)
	END_ODBC_COLUMN_MAP()

	DEFINE_ODBC_COMMAND(CFilesizeSelectAccessor, _T("SELECT `filesize` FROM `filename` WHERE `filename_id` = @last_filename_id;"))

		void ClearRecord() noexcept { memset(this, 0, sizeof(*this)); }
};

/**
 * @brief Executes a SELECT for the file size and returns it.
 */
class CFilesizeSelect : public CODBC::CCommand<CODBC::CAccessor<CFilesizeSelectAccessor>>
{
public:
	bool Iterate(const CODBC::CConnection& pDbConnect, ULONGLONG& nFileLength, _In_ bool bBind = true, _In_opt_ CODBC::SQL_ATTRIBUTE* pAttributes = nullptr, _In_ ULONG nAttributes = 0)
	{
		nFileLength = 0;
#pragma warning(suppress: 26477)
		SQLRETURN nRet{ Open(pDbConnect, GetDefaultCommand(), bBind, pAttributes, nAttributes) };
		ODBC_CHECK_RETURN_FALSE(nRet, m_Command);
		while (true)
		{
			ClearRecord();
			nRet = m_Command.FetchNext();
			if (!SQL_SUCCEEDED(nRet))
				break;
			nFileLength = m_nFilesize;
		}
		return true;
	}
};

/**
 * @brief ODBC accessor for selecting file data from the `filedata` table.
 */
class CFiledataSelectAccessor
{
public:
	TCHAR m_lpszContent[0x20000];
	__int64 m_nBase64;

	BEGIN_ODBC_PARAM_MAP(CFiledataSelectAccessor)
	END_ODBC_PARAM_MAP()

	BEGIN_ODBC_COLUMN_MAP(CFiledataSelectAccessor)
		ODBC_COLUMN_ENTRY(1, m_lpszContent)
		ODBC_COLUMN_ENTRY(2, m_nBase64)
	END_ODBC_COLUMN_MAP()

	DEFINE_ODBC_COMMAND(CFiledataSelectAccessor, _T("SELECT `content`, `base64` FROM `filedata` WHERE `filename_id` = @last_filename_id ORDER BY `filedata_id` ASC;"))

		void ClearRecord() noexcept { memset(this, 0, sizeof(*this)); }
};

/**
 * @brief Executes a SELECT for file data and streams it to the client socket.
 */
class CFiledataSelect : public CODBC::CCommand<CODBC::CAccessor<CFiledataSelectAccessor>>
{
public:
	bool Iterate(const CODBC::CConnection& pDbConnect, const int nSocketIndex, CWSocket& pApplicationSocket, SHA256& pSHA256, _In_ bool bBind = true, _In_opt_ CODBC::SQL_ATTRIBUTE* pAttributes = nullptr, _In_ ULONG nAttributes = 0)
	{
#pragma warning(suppress: 26477)
		SQLRETURN nRet{ Open(pDbConnect, GetDefaultCommand(), bBind, pAttributes, nAttributes) };
		ODBC_CHECK_RETURN_FALSE(nRet, m_Command);
		while (true)
		{
			ClearRecord();
			nRet = m_Command.FetchNext();
			if (!SQL_SUCCEEDED(nRet))
				break;
			TRACE(_T("m_nBase64 = %lld\n"), m_nBase64);
			std::string decoded = base64_decode(wstring_to_utf8(m_lpszContent));
			ASSERT((size_t)m_nBase64 == decoded.length());
			if (WriteBuffer(nSocketIndex, pApplicationSocket, (unsigned char*)decoded.data(), (int)decoded.length(), false, false))
			{
				pSHA256.update((unsigned char*)decoded.data(), decoded.length());
			}
			else
			{
				return false;
			}
		}
		return true;
	}
};

const int MAX_BUFFER = 0x10000;

/**
 * @brief Establishes a connection to the MySQL database using ODBC.
 *        Loads connection settings from the application configuration.
 * @return true on success, false on failure.
 */
bool ConnectToDatabase(CODBC::CEnvironment& pEnvironment, CODBC::CConnection& pConnection)
{
	CODBC::String sConnectionOutString;
	TCHAR sConnectionInString[0x100];

	std::wstring strHostName;
	int nHostPort = 3306;
	std::wstring strDatabase;
	std::wstring strUsername;
	std::wstring strPassword;
	if (!LoadAppSettings(strHostName, nHostPort, strDatabase, strUsername, strPassword))
		return false;
	const std::wstring strHostPort = utf8_to_wstring(std::to_string(nHostPort));

	SQLRETURN nRet = pEnvironment.Create();
	ODBC_CHECK_RETURN_FALSE(nRet, pEnvironment);

	nRet = pEnvironment.SetAttr(SQL_ATTR_ODBC_VERSION, SQL_OV_ODBC3_80);
	ODBC_CHECK_RETURN_FALSE(nRet, pEnvironment);

	nRet = pEnvironment.SetAttrU(SQL_ATTR_CONNECTION_POOLING, SQL_CP_DEFAULT);
	ODBC_CHECK_RETURN_FALSE(nRet, pEnvironment);

	nRet = pConnection.Create(pEnvironment);
	ODBC_CHECK_RETURN_FALSE(nRet, pConnection);

	_stprintf_s(sConnectionInString, _countof(sConnectionInString), _T("Driver={MySQL ODBC 8.0 Unicode Driver};Server=%s;Port=%s;Database=%s;User=%s;Password=%s;"),
		strHostName.c_str(), strHostPort.c_str(), strDatabase.c_str(), strUsername.c_str(), strPassword.c_str());
	nRet = pConnection.DriverConnect(const_cast<SQLTCHAR*>(reinterpret_cast<const SQLTCHAR*>(sConnectionInString)), sConnectionOutString);
	ODBC_CHECK_RETURN_FALSE(nRet, pConnection);

	return true;
}

/**
 * @brief Handles the download of a file from the server to a client.
 *        Streams file data from the database to the client socket, with SHA256 integrity check.
 * @param nSocketIndex Index of the client socket.
 * @param pApplicationSocket The socket to write to.
 * @param strFilePath The file path to download.
 * @return true on success, false on failure.
 */
bool DownloadFile(const int nSocketIndex, CWSocket& pApplicationSocket, const std::wstring& strFilePath)
{
	CODBC::CEnvironment pEnvironment;
	CODBC::CConnection pConnection;
	SHA256 pSHA256;

	std::array<CODBC::SQL_ATTRIBUTE, 2> attributes
	{ {
 #pragma warning(suppress: 26490)
	  { SQL_ATTR_CONCURRENCY,        reinterpret_cast<SQLPOINTER>(SQL_CONCUR_ROWVER), SQL_IS_INTEGER },
 #pragma warning(suppress: 26490)
	  { SQL_ATTR_CURSOR_SENSITIVITY, reinterpret_cast<SQLPOINTER>(SQL_INSENSITIVE),   SQL_IS_INTEGER }
	} };
#pragma warning(suppress: 26472)

	ULONGLONG nFileLength = 0;
	CFilenameSelect pFilenameSelect;
	CFilesizeSelect pFilesizeSelect;
	CFiledataSelect pFiledataSelect;
	TRACE(_T("[DownloadFile] %s\n"), strFilePath.c_str());
	if (!ConnectToDatabase(pEnvironment, pConnection) ||
		!pFilenameSelect.Execute(pConnection, strFilePath) ||
		!pFilesizeSelect.Iterate(pConnection, nFileLength, true, attributes.data(), static_cast<ULONG>(attributes.size())))
	{
		TRACE("MySQL operation failed!\n");
		return false;
	}

	TRACE(_T("nFileLength = %llu\n"), nFileLength);
	int nLength = sizeof(nFileLength);
	if (WriteBuffer(nSocketIndex, pApplicationSocket, (unsigned char*)&nFileLength, nLength, false, false))
	{
		if ((nFileLength > 0) &&
			!pFiledataSelect.Iterate(pConnection, nSocketIndex, pApplicationSocket, pSHA256, true, attributes.data(), static_cast<ULONG>(attributes.size())))
		{
			TRACE("MySQL operation failed!\n");
			return false;
		}
	}
	else
	{
		TRACE(_T("Invalid nFileLength!\n"));
		return false;
	}
	const std::string strDigestSHA256 = pSHA256.toString(pSHA256.digest());
	nLength = (int)strDigestSHA256.length() + 1;
	if (WriteBuffer(nSocketIndex, pApplicationSocket, (unsigned char*)strDigestSHA256.c_str(), nLength, false, true))
	{
		TRACE(_T("Download Done!\n"));
	}
	else
	{
		return false;
	}
	pConnection.Disconnect();
	return true;
}

/**
 * @brief Handles the upload of a file from a client to the server.
 *        Receives file data from the client socket and stores it in the database, with SHA256 integrity check.
 * @param nSocketIndex Index of the client socket.
 * @param pApplicationSocket The socket to read from.
 * @param strFilePath The file path to upload.
 * @return true on success, false on failure.
 */
bool UploadFile(const int nSocketIndex, CWSocket& pApplicationSocket, const std::wstring& strFilePath)
{
	CODBC::CEnvironment pEnvironment;
	CODBC::CConnection pConnection;
	SHA256 pSHA256;
	unsigned char pFileBuffer[MAX_BUFFER] = { 0, };

	CGenericStatement pGenericStatement;
	CFilenameInsert pFilenameInsert;
	CFilenameSelect pFilenameSelect;
	CFilenameUpdate pFilenameUpdate;
	CFiledataInsert pFiledataInsert;
	TRACE(_T("[UploadFile] %s\n"), strFilePath.c_str());
	if (!ConnectToDatabase(pEnvironment, pConnection))
	{
		TRACE("MySQL operation failed!\n");
		return false;
	}

	ULONGLONG nFileLength = 0;
	int nLength = (int)(sizeof(nFileLength) + 5);
	ZeroMemory(pFileBuffer, sizeof(pFileBuffer));
	if (ReadBuffer(nSocketIndex, pApplicationSocket, pFileBuffer, nLength, false, false))
	{
		CopyMemory(&nFileLength, &pFileBuffer[3], sizeof(nFileLength));
		TRACE(_T("nFileLength = %llu\n"), nFileLength);
		if (!pFilenameInsert.Execute(pConnection, strFilePath, nFileLength) ||
			!pGenericStatement.Execute(pConnection, _T("SET @last_filename_id = LAST_INSERT_ID()")))
		{
			if (!pFilenameSelect.Execute(pConnection, strFilePath) ||
				!pGenericStatement.Execute(pConnection, _T("DELETE FROM `filedata` WHERE `filename_id` = @last_filename_id")) ||
				!pFilenameUpdate.Execute(pConnection, nFileLength))
			{
				TRACE("MySQL operation failed!\n");
				return false;
			}
		}

		ULONGLONG nFileIndex = 0;
		while (nFileIndex < nFileLength)
		{
			nLength = (int)sizeof(pFileBuffer);
			ZeroMemory(pFileBuffer, sizeof(pFileBuffer));
			if (ReadBuffer(nSocketIndex, pApplicationSocket, pFileBuffer, nLength, false, false))
			{
				nFileIndex += (nLength - 5);
				pSHA256.update(&pFileBuffer[3], nLength - 5);

				std::string encoded = base64_encode(reinterpret_cast<const unsigned char*>(&pFileBuffer[3]), nLength - 5);
				if (!pFiledataInsert.Execute(pConnection, utf8_to_wstring(encoded), nLength - 5))
				{
					TRACE("MySQL operation failed!\n");
					return false;
				}
			}
			else
			{
				return false;
			}
		}
	}
	else
	{
		TRACE(_T("Invalid nFileLength!\n"));
		return false;
	}
	const std::string strDigestSHA256 = pSHA256.toString(pSHA256.digest());
	nLength = (int)strDigestSHA256.length() + 5;
	ZeroMemory(pFileBuffer, sizeof(pFileBuffer));
	if (ReadBuffer(nSocketIndex, pApplicationSocket, pFileBuffer, nLength, false, true))
	{
		const std::string strCommand = (char*)&pFileBuffer[3];
		if (strDigestSHA256.compare(strCommand) != 0)
		{
			TRACE(_T("Invalid SHA256!\n"));
			return false;
		}
	}
	pConnection.Disconnect();
	return true;
}

#define EOT 0x04

/**
 * @brief Handles the deletion of a file from the server database.
 *        Removes file data and metadata for the given file path.
 * @param nSocketIndex Index of the client socket (unused).
 * @param pApplicationSocket The socket to read EOT from.
 * @param strFilePath The file path to delete.
 * @return true on success, false on failure.
 */
bool DeleteFile(const int /*nSocketIndex*/, CWSocket& pApplicationSocket, const std::wstring& strFilePath)
{
	CODBC::CEnvironment pEnvironment;
	CODBC::CConnection pConnection;

	CGenericStatement pGenericStatement;
	CFilenameSelect pFilenameSelect;
	if (!ConnectToDatabase(pEnvironment, pConnection) ||
		!pFilenameSelect.Execute(pConnection, strFilePath) ||
		!pGenericStatement.Execute(pConnection, _T("DELETE FROM `filedata` WHERE `filename_id` = @last_filename_id")) ||
		!pGenericStatement.Execute(pConnection, _T("DELETE FROM `filename` WHERE `filename_id` = @last_filename_id")))
	{
		TRACE("MySQL operation failed!\n");
		return false;
	}

	unsigned char pFileBuffer[MAX_BUFFER] = { 0, };
	int nLength = sizeof(pFileBuffer);
	ZeroMemory(pFileBuffer, sizeof(pFileBuffer));
	if (((nLength = pApplicationSocket.Receive(pFileBuffer, nLength)) > 0) &&
		(EOT == pFileBuffer[nLength - 1]))
	{
		TRACE(_T("EOT Received\n"));
	}
	pConnection.Disconnect();
	return true;
}
