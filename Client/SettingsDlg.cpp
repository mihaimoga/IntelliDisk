/* This file is part of IntelliDisk application developed by Stefan-Mihai MOGA.

IntelliDisk is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Open
Source Initiative, either version 3 of the License, or any later version.

IntelliDisk is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
IntelliDisk. If not, see <http://www.opensource.org/licenses/gpl-3.0.html>*/

// CSettingsDlg.cpp : implementation file
//

#include "pch.h"
#include "IntelliDisk.h"
#include "SettingsDlg.h"
#include "IntelliDiskExt.h"

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
	DDX_Control(pDX, IDC_MACHINE_ID, m_ctrlMachineID);
	DDX_Control(pDX, IDC_SPECIAL_FOLDER, m_ctrlSpecialFolder);
	DDX_Control(pDX, IDC_SERVER_IP, m_ctrlServerIP);
	DDX_Control(pDX, IDC_SERVER_PORT, m_ctrlServerPort);
	DDX_Control(pDX, IDC_STARTUP_APPS, m_ctrlStartupApps);
}

BEGIN_MESSAGE_MAP(CSettingsDlg, CDialogEx)
	ON_BN_CLICKED(IDC_STARTUP_APPS, &CSettingsDlg::OnClickedStartupApps)
END_MESSAGE_MAP()

// CSettingsDlg message handlers

BOOL CSettingsDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_ctrlMachineID.SetWindowText(utf8_to_wstring(GetMachineID()).c_str());
	m_ctrlSpecialFolder.SetWindowText(GetSpecialFolder().c_str());
	CString strServerIP = theApp.GetString(_T("ServerIP"), IntelliDiskIP);
	const int nServer1 = strServerIP.Find(_T('.'), 0);
	const int nServer2 = strServerIP.Find(_T('.'), nServer1 + 1);
	const int nServer3 = strServerIP.Find(_T('.'), nServer2 + 1);
	m_ctrlServerIP.SetAddress((BYTE)_tstoi(strServerIP.Mid(0, nServer1)),
		(BYTE)_tstoi(strServerIP.Mid(nServer1 + 1, nServer2 - nServer1)),
		(BYTE)_tstoi(strServerIP.Mid(nServer2 + 1, nServer3 - nServer2)),
		(BYTE)_tstoi(strServerIP.Mid(nServer3 + 1, strServerIP.GetLength() - nServer3)));
	CString strServerPort;
	strServerPort.Format(_T("%d"), theApp.GetInt(_T("ServerPort"), IntelliDiskPort));
	m_ctrlServerPort.SetWindowText(strServerPort);
	m_ctrlServerPort.SetLimitText(5);
	m_ctrlStartupApps.SetCheck(theApp.GetInt(_T("StartupApps"), 0));

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CSettingsDlg::OnOK()
{
	CString strServerIP;
	BYTE nServer1, nServer2, nServer3, nServer4;
	m_ctrlServerIP.GetAddress(nServer1, nServer2, nServer3, nServer4);
	strServerIP.Format(_T("%d.%d.%d.%d"), nServer1, nServer2, nServer3, nServer4);
	theApp.WriteString(_T("ServerIP"), strServerIP);
	CString strServerPort;
	m_ctrlServerPort.GetWindowText(strServerPort);
	theApp.WriteInt(_T("ServerPort"), _tstoi(strServerPort));
	theApp.WriteInt(_T("StartupApps"), m_ctrlStartupApps.GetCheck());

	CDialogEx::OnOK();
}

void CSettingsDlg::OnClickedStartupApps()
{
	if (m_ctrlStartupApps.GetCheck())
	{
		if (InstallStartupApps(true))
		{
			MessageBox(_T("This application has been added successfully to Startup Apps!"), NULL, MB_OK | MB_ICONINFORMATION);
		}
	}
	else
	{
		if (InstallStartupApps(false))
		{
			MessageBox(_T("This application has been removed successfully from Startup Apps!"), NULL, MB_OK | MB_ICONINFORMATION);
		}
	}
}
