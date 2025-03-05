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

// SettingsDlg.cpp : implementation file
//

#include "pch.h"
#include "QuickTest.h"
#include "SettingsDlg.h"
#include "afxdialogex.h"
#include "../IntelliDiskExt.h"
#include "../IntelliDiskINI.h"
#include "../IntelliDiskSQL.h"
#include "../ODBCWrappers.h"
#include <string>

// CSettingsDlg dialog

IMPLEMENT_DYNAMIC(CSettingsDlg, CDialogEx)

CSettingsDlg::CSettingsDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SETTINGS_DIALOG, pParent)
{
}

CSettingsDlg::~CSettingsDlg()
{
}

void CSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_INTELLIDISK_PORT, m_ctrlServicePort);
	DDX_Control(pDX, IDC_MYSQL_HOSTNAME, m_ctrlHostName);
	DDX_Control(pDX, IDC_MYSQL_PORT, m_ctrlHostPort);
	DDX_Control(pDX, IDC_MYSQL_DATABSE, m_ctrlDatabase);
	DDX_Control(pDX, IDC_MYSQL_USERNAME, m_ctrlUsername);
	DDX_Control(pDX, IDC_MYSQL_PASSWORD, m_ctrlPassword);
	DDX_Control(pDX, IDC_STATUS, m_ctrlStatusMessage);
	DDX_Control(pDX, IDOK, m_ctrlOK);
	DDX_Control(pDX, IDCANCEL, m_ctrlCancel);
}

BEGIN_MESSAGE_MAP(CSettingsDlg, CDialogEx)
END_MESSAGE_MAP()

// CSettingsDlg message handlers

BOOL CSettingsDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_ctrlStatusMessage.SetWindowText(_T(""));

	// TODO:  Add extra initialization here
	const std::wstring strServicePort = utf8_to_wstring(std::to_string(LoadServicePort()));
	std::wstring strHostName;
	int nHostPort = 3306;
	std::wstring strDatabase;
	std::wstring strUsername;
	std::wstring strPassword;
	LoadAppSettings(strHostName, nHostPort, strDatabase, strUsername, strPassword);
	const std::wstring strHostPort = utf8_to_wstring(std::to_string(nHostPort));

	m_ctrlServicePort.SetWindowText(strServicePort.c_str());
	m_ctrlHostName.SetWindowText(strHostName.c_str());
	m_ctrlHostPort.SetWindowText(strHostPort.c_str());
	m_ctrlDatabase.SetWindowText(strDatabase.c_str());
	m_ctrlUsername.SetWindowText(strUsername.c_str());
	m_ctrlPassword.SetWindowText(strPassword.c_str());

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

//Another flavour of an ODBC_CHECK_RETURN macro
#define ODBC_CHECK_RETURN_FALSE(nRet, handle) \
	handle.ValidateReturnValue(nRet); \
	if (!SQL_SUCCEEDED(nRet)) \
{ \
	return false; \
}

bool CheckIfDatabaseConnected(const std::wstring& strHostName, const std::wstring& strHostPort, const std::wstring& strDatabase, const std::wstring& strUsername, const std::wstring& strPassword)
{
	CODBC::CEnvironment pEnvironment;
	CODBC::CConnection pConnection;
	CODBC::String sConnectionOutString;
	TCHAR sConnectionInString[0x100];

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

	CGenericStatement pGenericStatement;
	VERIFY(pGenericStatement.Execute(pConnection, _T("DROP TABLE IF EXISTS `filedata`;")));
	VERIFY(pGenericStatement.Execute(pConnection, _T("DROP TABLE IF EXISTS `filename`;")));
	VERIFY(pGenericStatement.Execute(pConnection, _T("CREATE TABLE `filename` (`filename_id` BIGINT NOT NULL AUTO_INCREMENT, `filepath` VARCHAR(256) NOT NULL, `filesize` BIGINT NOT NULL, PRIMARY KEY(`filename_id`)) ENGINE=InnoDB;")));
	VERIFY(pGenericStatement.Execute(pConnection, _T("CREATE TABLE `filedata` (`filedata_id` BIGINT NOT NULL AUTO_INCREMENT, `filename_id` BIGINT NOT NULL, `content` LONGTEXT NOT NULL, `base64` BIGINT NOT NULL, PRIMARY KEY(`filedata_id`), FOREIGN KEY filedata_fk(filename_id) REFERENCES filename(filename_id)) ENGINE=InnoDB;")));
	VERIFY(pGenericStatement.Execute(pConnection, _T("CREATE UNIQUE INDEX index_filepath ON `filename`(`filepath`);")));

	pConnection.Disconnect();
	return true;
}

void CSettingsDlg::OnOK()
{
	const int nMaxLength = 0x100;
	TCHAR lpszServicePort[nMaxLength] = { 0, };
	TCHAR lpszHostName[nMaxLength] = { 0, };
	TCHAR lpszHostPort[nMaxLength] = { 0, };
	TCHAR lpszDatabase[nMaxLength] = { 0, };
	TCHAR lpszUsername[nMaxLength] = { 0, };
	TCHAR lpszPassword[nMaxLength] = { 0, };

	m_ctrlOK.EnableWindow(FALSE);
	m_ctrlCancel.EnableWindow(FALSE);

	m_ctrlStatusMessage.SetWindowText(_T("Connecting..."));

	m_ctrlServicePort.GetWindowText(lpszServicePort, nMaxLength);
	m_ctrlHostName.GetWindowText(lpszHostName, nMaxLength);
	m_ctrlHostPort.GetWindowText(lpszHostPort, nMaxLength);
	m_ctrlDatabase.GetWindowText(lpszDatabase, nMaxLength);
	m_ctrlUsername.GetWindowText(lpszUsername, nMaxLength);
	m_ctrlPassword.GetWindowText(lpszPassword, nMaxLength);

	if (CheckIfDatabaseConnected(lpszHostName, lpszHostPort, lpszDatabase, lpszUsername, lpszPassword))
	{
		m_ctrlStatusMessage.SetWindowText(_T("Connected!"));
		Sleep(1000);

		VERIFY(SaveServicePort(std::stoi(lpszServicePort)));
		VERIFY(SaveAppSettings(lpszHostName, std::stoi(lpszHostPort), lpszDatabase, lpszUsername, lpszPassword));

		MessageBox(_T("Database connection was successful! Everything is OK now..."), _T("MySQL ODBC Connection"), MB_OK);

		CDialogEx::OnOK();
	}
	else
	{
		m_ctrlStatusMessage.SetWindowText(_T("Failed to connect!"));
		Sleep(10000);

		CDialogEx::OnCancel();
	}
}
