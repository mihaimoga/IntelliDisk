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

// QuickTestDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"

#include "QuickTest.h"
#include "QuickTestDlg.h"
#include "SettingsDlg.h"

#include "VersionInfo.h"
#include "HLinkCtrl.h"

#include "../IntelliDiskExt.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CAboutDlg dialog used for App About

/**
 * @brief About dialog class for displaying application information
 * @details Shows version, license, and contact information for the IntelliDisk server test application
 */
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();

protected:
	CStatic m_ctrlVersion;
	CEdit m_ctrlWarning;
	CVersionInfo m_pVersionInfo;
	CHLinkCtrl m_ctrlWebsite;
	CHLinkCtrl m_ctrlEmail;
	CHLinkCtrl m_ctrlContributors;

	DECLARE_MESSAGE_MAP()
};

/**
 * @brief Constructor for the About dialog
 */
CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

/**
 * @brief Exchanges data between dialog controls and member variables
 * @param pDX Pointer to a CDataExchange object
 */
void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_VERSION, m_ctrlVersion);
	DDX_Control(pDX, IDC_WARNING, m_ctrlWarning);
	DDX_Control(pDX, IDC_WEBSITE, m_ctrlWebsite);
	DDX_Control(pDX, IDC_EMAIL, m_ctrlEmail);
	DDX_Control(pDX, IDC_CONTRIBUTORS, m_ctrlContributors);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

/**
 * @brief Initializes the About dialog when it is first created
 * @details Loads version information from executable, displays license text, and sets up hyperlinks
 * @return TRUE to set focus to the first control, FALSE otherwise
 */
BOOL CAboutDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Split the help file path to get the directory of the executable
	// We need to find the .exe file to load its version resource
	TCHAR lpszDrive[_MAX_DRIVE];      // e.g., "C:"
	TCHAR lpszDirectory[_MAX_DIR];     // e.g., "\Program Files\IntelliDisk\"
	TCHAR lpszFilename[_MAX_FNAME];    // e.g., "QuickTest"
	TCHAR lpszExtension[_MAX_EXT];     // e.g., ".hlp"
	TCHAR lpszFullPath[0x1000 /* _MAX_PATH */];

	// Build path to executable by replacing extension with .exe
	// m_pszHelpFilePath typically points to .hlp file in same directory as .exe
	VERIFY(0 == _tsplitpath_s(AfxGetApp()->m_pszHelpFilePath, lpszDrive, _MAX_DRIVE, lpszDirectory, _MAX_DIR, lpszFilename, _MAX_FNAME, lpszExtension, _MAX_EXT));
	// Reconstruct path with .exe extension instead of .hlp
	VERIFY(0 == _tmakepath_s(lpszFullPath, 0x1000 /* _MAX_PATH */, lpszDrive, lpszDirectory, lpszFilename, _T(".exe")));

	// Load and display version information from executable
	if (m_pVersionInfo.Load(lpszFullPath))
	{
		CString strName = m_pVersionInfo.GetProductName().c_str();
		CString strVersion = m_pVersionInfo.GetProductVersionAsString().c_str();
		// Format version string: remove spaces and commas, keep only major.minor
		// Example: "1, 0, 0, 1" -> "1.0.0.1" -> "1.0"
		strVersion.Replace(_T(" "), _T(""));
		strVersion.Replace(_T(","), _T("."));
		const int nFirst = strVersion.Find(_T('.'));  // Find first dot (after major)
		const int nSecond = strVersion.Find(_T('.'), nFirst + 1);  // Find second dot (after minor)
		strVersion.Truncate(nSecond);  // Keep only major.minor
		// Display architecture-specific version string
#if _WIN32 || _WIN64
#if _WIN64
		m_ctrlVersion.SetWindowText(strName + _T(" version ") + strVersion + _T(" (64-bit)"));
#else
		m_ctrlVersion.SetWindowText(strName + _T(" version ") + strVersion + _T(" (32-bit)"));
#endif
#endif
	}

	// Display GPL v3 license text
	m_ctrlWarning.SetWindowText(_T("This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>."));

	// Setup hyperlinks for website, email, and contributors
	m_ctrlWebsite.SetHyperLink(_T("https://www.moga.doctor/"));
	m_ctrlEmail.SetHyperLink(_T("mailto:stefan-mihai@moga.doctor"));
	m_ctrlContributors.SetHyperLink(_T("https://github.com/mihaimoga/IntelliDisk/graphs/contributors"));

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

/**
 * @brief Handles cleanup when the About dialog is destroyed
 */
void CAboutDlg::OnDestroy()
{
	CDialog::OnDestroy();
}

// CQuickTestDlg dialog

/**
 * @brief Constructor for the QuickTest main dialog
 * @param pParent Pointer to the parent window
 */
CQuickTestDlg::CQuickTestDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_QUICKTEST_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

/**
 * @brief Exchanges data between dialog controls and member variables
 * @param pDX Pointer to a CDataExchange object
 */
void CQuickTestDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROGRESS, m_ctrlProgress);
	DDX_Control(pDX, IDC_STATUS, m_ctrlStatusMessage);
}

BEGIN_MESSAGE_MAP(CQuickTestDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_SETTINGS, &CQuickTestDlg::OnBnClickedSettings)
END_MESSAGE_MAP()

// CQuickTestDlg message handlers

/**
 * @brief Initializes the QuickTest dialog when it is first created
 * @details Adds "About" to system menu, starts the IntelliDisk server processing thread,
 *          and displays a marquee progress bar
 * @return TRUE to set focus to the first control, FALSE otherwise
 */
BOOL CQuickTestDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	// System command IDs are masked with 0xFFF0 to ignore low 4 bits
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	// System commands must be < 0xF000 (Windows reserved range)
	ASSERT(IDM_ABOUTBOX < 0xF000);

	// Add "About" item to system menu (accessed via Alt+Space)
	CMenu* pSysMenu = GetSystemMenu(FALSE);  // FALSE = get current menu, not default
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		// Load localized "About" string from resources
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			// Add separator and About menu item
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// Initialize status message and start server
	m_ctrlStatusMessage.SetWindowText(_T(""));
	// Start IntelliDisk server processing thread (accepts client connections)
	StartProcessingThread();
	// Show animated progress bar to indicate server is running
	// Marquee mode: continuous animation, 30ms update interval
	m_ctrlProgress.SetMarquee(TRUE, 30);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

/**
 * @brief Handles system commands (e.g., About menu)
 * @param nID Command ID
 * @param lParam Additional message-specific information
 */
void CQuickTestDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	// Check if this is our custom About command
	// Mask with 0xFFF0 to ignore low 4 bits (reserved for system use)
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		// Show About dialog when user selects it from system menu
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		// Pass all other system commands to base class
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

/**
 * @brief Handles painting the dialog
 * @details When minimized, draws the application icon centered in the window
 */
void CQuickTestDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		// Erase background behind icon
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		// Get system metrics for standard icon size
		int cxIcon = GetSystemMetrics(SM_CXICON);  // Icon width in pixels
		int cyIcon = GetSystemMetrics(SM_CYICON);  // Icon height in pixels
		CRect rect;
		GetClientRect(&rect);
		// Calculate center position (+1 for rounding)
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon at calculated center position
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		// Not minimized - use default painting
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.

/**
 * @brief Returns the cursor to display while dragging the minimized window
 * @return Handle to the application icon
 */
HCURSOR CQuickTestDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

/**
 * @brief Handles cleanup when the dialog is destroyed
 * @details Stops the IntelliDisk server processing thread before closing
 */
void CQuickTestDlg::OnDestroy()
{
	// Stop IntelliDisk server (closes all client connections and threads)
	StopProcessingThread();

	CDialog::OnDestroy();
}

/**
 * @brief Handles the Settings button click event
 * @details Displays the settings dialog and restarts the server if settings changed
 */
void CQuickTestDlg::OnBnClickedSettings()
{
	CSettingsDlg dlgSettings(this);
	if (dlgSettings.DoModal() == IDOK)
	{
		// Settings changed - restart server with new configuration
		// === SERVER RESTART SEQUENCE ===

		// Step 1: Update UI to show restart in progress
		m_ctrlStatusMessage.SetWindowText(_T("Restarting..."));
		m_ctrlProgress.SetMarquee(FALSE, 30);  // Stop animation during restart

		// Step 2: Gracefully shutdown server (closes all client connections)
		StopProcessingThread();

		// Step 3: Wait for all server threads to exit cleanly
		// This ensures sockets are closed and resources are released
		Sleep(1000);  // 1 second grace period

		// Step 4: Restart server with new settings from XML config file
		StartProcessingThread();

		// Step 5: Restore UI to normal running state
		m_ctrlProgress.SetMarquee(TRUE, 30);  // Resume animation
		m_ctrlStatusMessage.SetWindowText(_T(""));  // Clear status
	}
}
