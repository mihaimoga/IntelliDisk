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

// CSettingsDlg.cpp : implementation file
//

#include "pch.h"
#include "IntelliDisk.h"
#include "SettingsDlg.h"
#include "IntelliDiskExt.h"

// CSettingsDlg dialog

IMPLEMENT_DYNAMIC(CSettingsDlg, CDialogEx)

/**
 * @brief Constructor for the Settings dialog
 * @param pParent Pointer to the parent window
 */
CSettingsDlg::CSettingsDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SETTINGS_DIALOG, pParent)
{
}

/**
 * @brief Destructor for the Settings dialog
 */
CSettingsDlg::~CSettingsDlg()
{
}

/**
 * @brief Exchanges data between dialog controls and member variables
 * @param pDX Pointer to a CDataExchange object
 */
void CSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	// Bind dialog controls to member variables for easy access
	DDX_Control(pDX, IDC_MACHINE_ID, m_ctrlMachineID);          // Read-only: Shows unique machine identifier
	DDX_Control(pDX, IDC_SPECIAL_FOLDER, m_ctrlSpecialFolder);  // Read-only: Shows IntelliDisk sync folder path
	DDX_Control(pDX, IDC_SERVER_IP, m_ctrlServerIP);            // IP address control for server connection
	DDX_Control(pDX, IDC_SERVER_PORT, m_ctrlServerPort);        // Edit control for server port (1-65535)
	DDX_Control(pDX, IDC_STARTUP_APPS, m_ctrlStartupApps);      // Checkbox for Windows startup configuration
}

BEGIN_MESSAGE_MAP(CSettingsDlg, CDialogEx)
	ON_BN_CLICKED(IDC_STARTUP_APPS, &CSettingsDlg::OnClickedStartupApps)
END_MESSAGE_MAP()

// CSettingsDlg message handlers

/**
 * @brief Initializes the Settings dialog when it is first created
 * @details Loads and displays machine ID, special folder, server IP, server port, and startup apps settings
 * @return TRUE to set focus to the first control, FALSE otherwise
 */
BOOL CSettingsDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Display unique machine identifier (read-only)
	// Format: "username:computername"
	m_ctrlMachineID.SetWindowText(utf8_to_wstring(GetMachineID()).c_str());

	// Display IntelliDisk sync folder path (read-only)
	// Typically: C:\Users\<username>\IntelliDisk
	m_ctrlSpecialFolder.SetWindowText(GetSpecialFolder().c_str());

	// === LOAD SERVER IP ADDRESS ===
	// Parse dotted-decimal IP address string into 4 octets
	// Example: "192.168.1.100" -> 192, 168, 1, 100
	CString strServerIP = theApp.GetString(_T("ServerIP"), IntelliDiskIP);  // Load from registry or use default
	const int nServer1 = strServerIP.Find(_T('.'), 0);  // Position of first dot
	const int nServer2 = strServerIP.Find(_T('.'), nServer1 + 1);  // Position of second dot
	const int nServer3 = strServerIP.Find(_T('.'), nServer2 + 1);  // Position of third dot
	// Set IP address control with 4 octets
	m_ctrlServerIP.SetAddress(
		(BYTE)_tstoi(strServerIP.Mid(0, nServer1)),  // First octet (0 to first dot)
		(BYTE)_tstoi(strServerIP.Mid(nServer1 + 1, nServer2 - nServer1)),  // Second octet
		(BYTE)_tstoi(strServerIP.Mid(nServer2 + 1, nServer3 - nServer2)),  // Third octet
		(BYTE)_tstoi(strServerIP.Mid(nServer3 + 1, strServerIP.GetLength() - nServer3)));  // Fourth octet

	// === LOAD SERVER PORT ===
	CString strServerPort;
	strServerPort.Format(_T("%d"), theApp.GetInt(_T("ServerPort"), IntelliDiskPort));  // Load from registry or use default
	m_ctrlServerPort.SetWindowText(strServerPort);
	m_ctrlServerPort.SetLimitText(5);  // Maximum 5 digits (1-65535)

	// === LOAD STARTUP APPS SETTING ===
	// Check checkbox if application is configured to run at Windows startup
	m_ctrlStartupApps.SetCheck(theApp.GetInt(_T("StartupApps"), 0));  // 0 = unchecked (default)

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

/**
 * @brief Handles the OK button click event
 * @details Saves server IP, server port, and startup apps settings to application configuration
 */
void CSettingsDlg::OnOK()
{
	// === SAVE SERVER IP ADDRESS ===
	// Get 4 octets from IP address control
	CString strServerIP;
	BYTE nServer1, nServer2, nServer3, nServer4;
	m_ctrlServerIP.GetAddress(nServer1, nServer2, nServer3, nServer4);
	// Format as dotted-decimal string (e.g., "192.168.1.100")
	strServerIP.Format(_T("%d.%d.%d.%d"), nServer1, nServer2, nServer3, nServer4);
	// Save to registry: HKCU\Software\Mihai Moga\IntelliDisk\ServerIP
	theApp.WriteString(_T("ServerIP"), strServerIP);

	// === SAVE SERVER PORT ===
	CString strServerPort;
	m_ctrlServerPort.GetWindowText(strServerPort);
	// Save to registry: HKCU\Software\Mihai Moga\IntelliDisk\ServerPort
	theApp.WriteInt(_T("ServerPort"), _tstoi(strServerPort));

	// === SAVE STARTUP APPS SETTING ===
	// Save checkbox state to registry: HKCU\Software\Mihai Moga\IntelliDisk\StartupApps
	theApp.WriteInt(_T("StartupApps"), m_ctrlStartupApps.GetCheck());

	// Close dialog and return IDOK to parent
	CDialogEx::OnOK();
}

/**
 * @brief Handles the Startup Apps checkbox click event
 * @details Adds or removes the application from Windows startup based on checkbox state
 * 
 * IMMEDIATE ACTION PATTERN:
 * =========================
 * Unlike other settings which are saved on OK, this checkbox
 * applies changes immediately when clicked. This provides instant
 * feedback and avoids confusion about whether the change was applied.
 */
void CSettingsDlg::OnClickedStartupApps()
{
	if (m_ctrlStartupApps.GetCheck())  // Checkbox is now checked
	{
		// Add to Windows startup (modify registry Run key)
		if (InstallStartupApps(true))
		{
			MessageBox(_T("This application has been added successfully to Startup Apps!"), NULL, MB_OK | MB_ICONINFORMATION);
		}
	}
	else  // Checkbox is now unchecked
	{
		// Remove from Windows startup (delete registry Run key entry)
		if (InstallStartupApps(false))
		{
			MessageBox(_T("This application has been removed successfully from Startup Apps!"), NULL, MB_OK | MB_ICONINFORMATION);
		}
	}
}
