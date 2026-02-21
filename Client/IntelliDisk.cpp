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

// IntelliDisk.cpp : Defines the class behaviors for the application.
//

#include "pch.h"
#include "framework.h"

#include "IntelliDisk.h"
#include "MainFrame.h"
#include "IntelliDiskExt.h"

#include "VersionInfo.h"
#include "HLinkCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CIntelliDiskApp

BEGIN_MESSAGE_MAP(CIntelliDiskApp, CWinAppEx)
	ON_COMMAND(ID_APP_ABOUT, &CIntelliDiskApp::OnAppAbout)
END_MESSAGE_MAP()


// CIntelliDiskApp construction

/**
 * @brief Constructor for the IntelliDisk application
 * @details Initializes Restart Manager support and single-instance checker
 */
CIntelliDiskApp::CIntelliDiskApp() noexcept : m_pInstanceChecker(_T("IntelliDisk"))
{
	// Enable Restart Manager for automatic recovery after crashes/updates
	// Supports: restart, recovery, and graceful shutdown during Windows updates
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_ALL_ASPECTS;
#ifdef _MANAGED
	// If the application is built using Common Language Runtime support (/clr):
	//     1) This additional setting is needed for Restart Manager support to work properly.
	//     2) In your project, you must add a reference to System.Windows.Forms in order to build.
	System::Windows::Forms::Application::SetUnhandledExceptionMode(System::Windows::Forms::UnhandledExceptionMode::ThrowException);
#endif

	// Set unique application ID for Windows 7+ taskbar grouping and jump lists
	// Format: CompanyName.ProductName.SubProduct.VersionInformation
	// TODO: replace application ID string below with unique ID string; recommended
	// format for string is CompanyName.ProductName.SubProduct.VersionInformation
	SetAppID(_T("IntelliDisk.AppID.NoVersion"));

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

// The one and only CIntelliDiskApp object

CIntelliDiskApp theApp;


// CIntelliDiskApp initialization

/**
 * @brief Initializes the application instance
 * @return TRUE if initialization succeeds, FALSE otherwise
 * 
 * INITIALIZATION SEQUENCE:
 * ========================
 * 1. Initialize common controls (Windows UI components)
 * 2. Initialize WinSock (for network communication)
 * 3. Initialize OLE (for COM/ActiveX support)
 * 4. Check for previous instance (single-instance enforcement)
 * 5. Setup MFC managers (context menu, keyboard, tooltip)
 * 6. Create/verify IntelliDisk working directory
 * 7. Create main frame window (hidden initially, runs in tray)
 * 8. Initialize COM for apartment-threaded model
 * 9. Register as first instance for future detection
 */
BOOL CIntelliDiskApp::InitInstance()
{
	// === STEP 1: INITIALIZE COMMON CONTROLS ===
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;  // Standard Windows 95 controls
	InitCommonControlsEx(&InitCtrls);

	CWinAppEx::InitInstance();

	// === STEP 2: INITIALIZE WINSOCK ===
	// Required for all network socket operations (client-server communication)
	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	// === STEP 3: INITIALIZE OLE LIBRARIES ===
	// Required for COM objects, drag-and-drop, and clipboard operations
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	// Enable container for ActiveX controls
	AfxEnableControlContainer();

	// Disable Windows 7+ taskbar thumbnail preview (runs in tray)
	EnableTaskbarInteraction(FALSE);

	// Initialize RichEdit 2.0 control (for formatted text)
	// AfxInitRichEdit2() is required to use RichEdit control
	AfxInitRichEdit2();

	// === STEP 4: CHECK FOR PREVIOUS INSTANCE ===
	// IntelliDisk should only run one instance at a time
	// If another instance exists, activate it and exit this one
	//Check for the previous instance as soon as possible
	if (m_pInstanceChecker.PreviousInstanceRunning())
	{
		// Parse command line to check if a file was passed
		CCommandLineInfo cmdInfo;
		ParseCommandLine(cmdInfo);

		// Notify user and activate the existing instance
		AfxMessageBox(_T("Previous version detected, will now restore it..."), MB_OK | MB_ICONINFORMATION);
		// Bring existing instance to foreground and optionally open file
		m_pInstanceChecker.ActivatePreviousInstance(cmdInfo.m_strFileName);
		return FALSE;  // Exit this instance
	}

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Mihai Moga"));  // Settings stored in HKCU\Software\Mihai Moga\IntelliDisk

	// === STEP 5: INITIALIZE MFC MANAGERS ===
	// Context menu manager: handles right-click menus
	InitContextMenuManager();

	// Keyboard manager: handles keyboard shortcuts and accelerators
	InitKeyboardManager();

	// Tooltip manager: handles hover tooltips with custom styling
	InitTooltipManager();
	CMFCToolTipInfo ttParams;
	ttParams.m_bVislManagerTheme = TRUE;  // Use visual manager theme for tooltips
	theApp.GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL,
		RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);

	// === STEP 6: CREATE/VERIFY INTELLIDISK WORKING DIRECTORY ===
	// Set working directory to %USERPROFILE%\IntelliDisk\
	// This is where synchronized files are stored
	if (!SetCurrentDirectory(GetSpecialFolder().c_str()))
	{
		// Directory doesn't exist - create it
		VERIFY(CreateDirectory(GetSpecialFolder().c_str(), nullptr));
		VERIFY(SetCurrentDirectory(GetSpecialFolder().c_str()));
	}

	// === STEP 7: CREATE MAIN FRAME WINDOW ===
	// To create the main window, this code creates a new frame window
	// object and then sets it as the application's main window object
	CFrameWnd* pFrame = new CMainFrame;
	if (!pFrame)
		return FALSE;
	m_pMainWnd = pFrame;
	// create and load the frame with its resources
	pFrame->LoadFrame(IDR_MAINFRAME,
		WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, nullptr,
		nullptr);

	// === STEP 8: INITIALIZE COM FOR APARTMENT-THREADED MODEL ===
	// Required for XML settings file access and other COM operations
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr))
	{
		AfxMessageBox(_T("COM initialization failed."), MB_OK | MB_ICONERROR);
		return FALSE;
	}

	// The one and only window has been initialized, so show and update it
	// Note: Window is hidden (SW_HIDE) - application runs in system tray
	pFrame->ShowWindow(SW_HIDE);
	pFrame->UpdateWindow();
	// pFrame->MoveWindow(CRect(0, 0, 1214, 907));
	// pFrame->CenterWindow();

	// === STEP 9: REGISTER AS FIRST INSTANCE ===
	// If this is the first instance of our App then track it so any other instances can find us
	m_pInstanceChecker.TrackFirstInstanceRunning(m_pMainWnd->GetSafeHwnd());

	return TRUE;
}

/**
 * @brief Handles application exit and cleanup
 * @return Exit code for the application
 */
int CIntelliDiskApp::ExitInstance()
{
	// Cleanup OLE libraries (uninitialize COM)
	//TODO: handle additional resources you may have added
	AfxOleTerm(FALSE);

	return CWinAppEx::ExitInstance();
}

// CIntelliDiskApp message handlers

// CAboutDlg dialog used for App About

/**
 * @brief Dialog class for displaying application information
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
 * @brief Retrieves the full path of the current module (executable)
 * @param pdwLastError Optional pointer to receive the last error code
 * @return The full path of the module, or an empty string on failure
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
 * @brief Initializes the About dialog when it is first created
 * @return TRUE to set focus to the first control, FALSE otherwise
 */
BOOL CAboutDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Get full path to current executable for version loading
	CString strFullPath{ GetModuleFileName() };
	if (strFullPath.IsEmpty())
#pragma warning(suppress: 26487)
		return FALSE;

	// Load and display version information from executable
	if (m_pVersionInfo.Load(strFullPath.GetString()))
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

/**
 * @brief Displays the About dialog
 */
void CIntelliDiskApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

// CIntelliDiskApp customization load/save methods

/**
 * @brief Performs actions before loading the application state
 */
void CIntelliDiskApp::PreLoadState()
{
	BOOL bNameValid;
	CString strName;
	bNameValid = strName.LoadString(IDS_EDIT_MENU);
	ASSERT(bNameValid);
	GetContextMenuManager()->AddMenu(strName, IDR_POPUP_EDIT);
}

/**
 * @brief Loads custom application state from persistent storage
 */
void CIntelliDiskApp::LoadCustomState()
{
}

/**
 * @brief Saves custom application state to persistent storage
 */
void CIntelliDiskApp::SaveCustomState()
{
}

// CIntelliDiskApp message handlers
