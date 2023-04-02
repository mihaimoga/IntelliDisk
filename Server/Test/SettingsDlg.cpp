/* This file is part of IntelliDisk application developed by Stefan-Mihai MOGA.

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
}

BEGIN_MESSAGE_MAP(CSettingsDlg, CDialogEx)
END_MESSAGE_MAP()

// CSettingsDlg message handlers

BOOL CSettingsDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

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

void CSettingsDlg::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class
	const int nMaxLength = 0x100;
	TCHAR lpszServicePort[nMaxLength] = { 0, };
	TCHAR lpszHostName[nMaxLength] = { 0, };
	TCHAR lpszHostPort[nMaxLength] = { 0, };
	TCHAR lpszDatabase[nMaxLength] = { 0, };
	TCHAR lpszUsername[nMaxLength] = { 0, };
	TCHAR lpszPassword[nMaxLength] = { 0, };

	m_ctrlServicePort.GetWindowText(lpszServicePort, nMaxLength);
	m_ctrlHostName.GetWindowText(lpszHostName, nMaxLength);
	m_ctrlHostPort.GetWindowText(lpszHostPort, nMaxLength);
	m_ctrlDatabase.GetWindowText(lpszDatabase, nMaxLength);
	m_ctrlUsername.GetWindowText(lpszUsername, nMaxLength);
	m_ctrlPassword.GetWindowText(lpszPassword, nMaxLength);

	VERIFY(SaveServicePort(std::stoi(lpszServicePort)));
	VERIFY(SaveAppSettings(lpszHostName, std::stoi(lpszHostPort), lpszDatabase, lpszUsername, lpszPassword));

	CDialogEx::OnOK();
}
