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

// MainFrame.cpp : implementation of the CMainFrame class
//

#include "pch.h"
#include "framework.h"

#include "IntelliDisk.h"
#include "MainFrame.h"
#include "SettingsDlg.h"
#include "IntelliDiskExt.h"
#include "WebBrowserDlg.h"
#include "CheckForUpdatesDlg.h"

// Custom message ID for tray icon notifications (right-click, double-click, etc.)
#define WM_TRAYNOTIFY WM_USER + 0x1234

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWndEx)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWndEx)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SETFOCUS()
	ON_COMMAND(ID_FILE_PRINT, &CMainFrame::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, &CMainFrame::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CMainFrame::OnFilePrintPreview)
	ON_UPDATE_COMMAND_UI(ID_FILE_PRINT_PREVIEW, &CMainFrame::OnUpdateFilePrintPreview)
	ON_MESSAGE(WM_TRAYNOTIFY, OnTrayNotification)
	ON_COMMAND(ID_SHOW_APPLICATION, &CMainFrame::OnShowApplication)
	ON_COMMAND(ID_HIDE_APPLICATION, &CMainFrame::OnHideApplication)
	ON_COMMAND(ID_SETTINGS, &CMainFrame::OnSettings)
	ON_COMMAND(ID_OPEN_FOLDER, &CMainFrame::OnOpenFolder)
	ON_COMMAND(ID_VIEW_ONLINE, &CMainFrame::OnViewOnline)
	ON_COMMAND(IDC_TWITTER, &CMainFrame::OnTwitter)
	ON_COMMAND(IDC_LINKEDIN, &CMainFrame::OnLinkedin)
	ON_COMMAND(IDC_FACEBOOK, &CMainFrame::OnFacebook)
	ON_COMMAND(IDC_INSTAGRAM, &CMainFrame::OnInstagram)
	ON_COMMAND(IDC_ISSUES, &CMainFrame::OnIssues)
	ON_COMMAND(IDC_DISCUSSIONS, &CMainFrame::OnDiscussions)
	ON_COMMAND(IDC_WIKI, &CMainFrame::OnWiki)
	ON_COMMAND(IDC_USER_MANUAL, &CMainFrame::OnUserManual)
	ON_COMMAND(IDC_CHECK_FOR_UPDATES, &CMainFrame::OnCheckForUpdates)
END_MESSAGE_MAP()

// CMainFrame construction/destruction

/**
 * @brief Constructor for the main frame window
 * @details Initializes synchronization objects (semaphores, mutexes) and member variables
 * 
 * SYNCHRONIZATION INITIALIZATION:
 * ================================
 * Creates producer-consumer pattern synchronization primitives:
 * - hOccupiedSemaphore: Count = 0 (initially no items in queue)
 * - hEmptySemaphore: Count = NOTIFY_FILE_SIZE (all slots available)
 * - hResourceMutex: Binary semaphore (count = 1) for queue protection
 * - hSocketMutex: Binary semaphore (count = 1) for socket protection
 */
CMainFrame::CMainFrame() noexcept
{
	// Load tray icon for system tray notification area
	m_hMainFrameIcon = CTrayNotifyIcon::LoadIcon(IDR_MAINFRAME);
	m_MainButton = nullptr;
	// Initialize producer-consumer semaphores for thread-safe queue
	m_hOccupiedSemaphore = CreateSemaphore(nullptr, 0, NOTIFY_FILE_SIZE, nullptr);  // Items in queue
	m_hEmptySemaphore = CreateSemaphore(nullptr, NOTIFY_FILE_SIZE, NOTIFY_FILE_SIZE, nullptr);  // Free slots
	m_hResourceMutex = CreateSemaphore(nullptr, 1, 1, nullptr);  // Queue mutex
	m_hSocketMutex = CreateSemaphore(nullptr, 1, 1, nullptr);  // Socket mutex
	// Initialize circular queue pointers
	m_nNextIn = m_nNextOut = 0;
}

/**
 * @brief Destructor for the main frame window
 * @details Cleans up synchronization objects (semaphores, mutexes)
 * 
 * Note: Cleanup order matters - threads should already be stopped
 * before destructor is called (handled in OnDestroy)
 */
CMainFrame::~CMainFrame()
{
	// Clean up synchronization objects in reverse order of dependency
	if (m_hSocketMutex != nullptr)
	{
		VERIFY(CloseHandle(m_hSocketMutex));
		m_hSocketMutex = nullptr;
	}

	if (m_hResourceMutex != nullptr)
	{
		VERIFY(CloseHandle(m_hResourceMutex));
		m_hResourceMutex = nullptr;
	}

	if (m_hEmptySemaphore != nullptr)
	{
		VERIFY(CloseHandle(m_hEmptySemaphore));
		m_hEmptySemaphore = nullptr;
	}

	if (m_hOccupiedSemaphore != nullptr)
	{
		VERIFY(CloseHandle(m_hOccupiedSemaphore));
		m_hOccupiedSemaphore = nullptr;
	}
}

/**
 * @brief Handles the window creation event
 * @param lpCreateStruct Pointer to CREATESTRUCT containing window creation parameters
 * @return 0 on success, -1 on failure
 */
int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
		return -1;

	// === PHASE 1: CREATE MAIN VIEW ===
	// Create list control view to display file operation messages
	if (!m_wndView.Create(nullptr, nullptr, AFX_WS_DEFAULT_VIEW, CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, nullptr))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}

	// === PHASE 2: CONFIGURE MFC DOCKING AND VISUAL STYLE ===
	// Enable Visual Studio 2005 style docking window behavior
	CDockingManager::SetDockingMode(DT_SMART);
	// Enable Visual Studio 2005 style docking window auto-hide behavior
	EnableAutoHidePanes(CBRS_ALIGN_ANY);

	// Set the visual manager used to draw all user interface elements
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// === PHASE 3: CREATE AND CONFIGURE RIBBON BAR ===
	m_wndRibbonBar.Create(this);
	m_wndRibbonBar.LoadFromResource(IDR_RIBBON);

	// Hide the application button (Office-style menu)
	m_MainButton = new CMFCRibbonApplicationButton;
	m_MainButton->SetVisible(FALSE);
	m_wndRibbonBar.SetApplicationButton(m_MainButton, CSize());

	// === PHASE 4: CREATE SYSTEM TRAY ICON ===
	if (!m_pTrayIcon.Create(this, IDR_TRAYPOPUP, _T("IntelliDisk"), m_hMainFrameIcon, WM_TRAYNOTIFY))
	{
		AfxMessageBox(_T("Failed to create tray icon"), MB_OK | MB_ICONSTOP);
		return -1;
	}

	// === PHASE 5: SETUP FILE ICON IMAGE LIST ===
	// Get system image list for file type icons
	SHFILEINFO pshFileInfo = { 0 };
	HIMAGELIST hSystemImageList =
		(HIMAGELIST)SHGetFileInfo(
			_T(""),
			0,
			&pshFileInfo,
			sizeof(pshFileInfo),
			SHGFI_ICON | SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	if (hSystemImageList != nullptr)
	{
		// Attach system image list to display file type icons in list view
		VERIFY(m_pImageList.Attach(hSystemImageList));
		m_wndView.GetListCtrl().SetImageList(&m_pImageList, LVSIL_SMALL);
	}

	// === PHASE 6: CONFIGURE LIST VIEW COLUMNS ===
	CRect rectClient;
	m_wndView.GetListCtrl().GetClientRect(&rectClient);
	// Single column to show operation messages (Upload, Download, Delete)
	m_wndView.GetListCtrl().InsertColumn(0, _T("Operation"), LVCFMT_LEFT, rectClient.Width());

	// === PHASE 7: START DIRECTORY MONITORING ===
	// Monitor IntelliDisk folder for file changes
	m_pNotifyDirCheck.SetDirectory(GetSpecialFolder().c_str());
	m_pNotifyDirCheck.SetData(this);  // Pass this pointer to callback
	// Set callback to work with each new file system event
	m_pNotifyDirCheck.SetActionCallback(DirCallback);
	m_pNotifyDirCheck.Run();  // Start monitoring thread

	// === PHASE 8: LOAD SERVER CONNECTION SETTINGS ===
	// Load server IP and port from registry (or use defaults)
	m_strServerIP = theApp.GetString(_T("ServerIP"), IntelliDiskIP);
	m_nServerPort = theApp.GetInt(_T("ServerPort"), IntelliDiskPort);

	// === PHASE 9: START WORKER THREADS ===
	// Producer thread: Handles server connection and incoming commands
	m_hProducerThread = CreateThread(nullptr, 0, ProducerThread, this, 0, &m_dwThreadID[0]);
	ASSERT(m_hProducerThread != nullptr);
	// Consumer thread: Processes file operations from queue
	m_hConsumerThread = CreateThread(nullptr, 0, ConsumerThread, this, 0, &m_dwThreadID[1]);
	ASSERT(m_hConsumerThread != nullptr);

	return 0;
}

/**
 * @brief Handles the window destruction event
 * @details Stops directory monitoring, signals threads to stop, and waits for their completion
 * 
 * GRACEFUL SHUTDOWN SEQUENCE:
 * ===========================
 * 1. Stop directory monitoring to prevent new file events
 * 2. Queue ID_STOP_PROCESS event to signal threads to exit
 * 3. Wait for both threads to complete (producer and consumer)
 * 4. Clean up thread handles
 * 
 * Note: Synchronization objects are cleaned up in destructor
 */
void CMainFrame::OnDestroy()
{
	// === STEP 1: STOP DIRECTORY MONITORING ===
	// Stop watching for file system changes
	m_pNotifyDirCheck.Stop();

	// === STEP 2: SIGNAL THREADS TO STOP ===
	// Add stop command to queue (consumer thread will process it)
	AddNewItem(ID_STOP_PROCESS, std::wstring(_T("")), this);

	// === STEP 3: WAIT FOR THREADS TO COMPLETE ===
	// Wait for both producer and consumer threads to exit gracefully
	HANDLE hThreadArray[2] = { 0, 0 };
	hThreadArray[0] = m_hProducerThread;
	hThreadArray[1] = m_hConsumerThread;
	WaitForMultipleObjects(2, hThreadArray, TRUE, INFINITE);  // Wait for all threads

	// === STEP 4: CLEAN UP THREAD HANDLES ===
	if (m_hProducerThread != nullptr)
	{
		VERIFY(CloseHandle(m_hProducerThread));
		m_hProducerThread = nullptr;
	}

	if (m_hConsumerThread != nullptr)
	{
		VERIFY(CloseHandle(m_hConsumerThread));
		m_hConsumerThread = nullptr;
	}

	CFrameWndEx::OnDestroy();
}

/**
 * @brief Called before window creation to modify window properties
 * @param cs Reference to CREATESTRUCT to be modified
 * @return TRUE if window creation should continue, FALSE otherwise
 */
BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if (!CFrameWndEx::PreCreateWindow(cs))
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	// Remove client edge (3D border) for modern flat appearance
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	// Register custom window class (needed for proper message handling)
	cs.lpszClass = AfxRegisterWndClass(0);
	return TRUE;
}

// CMainFrame diagnostics

#ifdef _DEBUG
/**
 * @brief Validates the object's state (debug builds only)
 */
void CMainFrame::AssertValid() const
{
	CFrameWndEx::AssertValid();
}

/**
 * @brief Dumps the object's contents to a dump context (debug builds only)
 * @param dc Reference to CDumpContext for output
 */
void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWndEx::Dump(dc);
}
#endif //_DEBUG

// CMainFrame message handlers

/**
 * @brief Handles the focus event by forwarding it to the view window
 * @param pOldWnd Pointer to the window that lost focus (unused)
 */
void CMainFrame::OnSetFocus(CWnd* /*pOldWnd*/)
{
	// forward focus to the view window
	m_wndView.SetFocus();
}

/**
 * @brief Routes command messages to appropriate handlers
 * @param nID Command ID
 * @param nCode Notification code
 * @param pExtra Additional data for the command
 * @param pHandlerInfo Command handler information
 * @return TRUE if message was handled, FALSE otherwise
 * 
 * COMMAND ROUTING PRIORITY:
 * =========================
 * 1. View window (m_wndView) - gets first chance
 * 2. Frame window (this) - handles if view doesn't
 * 3. Parent chain - continues up if neither handles it
 */
BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// let the view have first crack at the command
	// This allows the list control to handle commands like copy/paste
	if (m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return TRUE;

	// otherwise, do default handling (frame window and parent chain)
	return CFrameWndEx::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

/**
 * @brief Handles tray icon notifications
 * @param wParam Additional message-specific information
 * @param lParam Additional message-specific information
 * @return 0 to indicate message was processed
 * 
 * TRAY ICON EVENTS:
 * =================
 * - Left-click: (handled by CTrayNotifyIcon - typically restores window)
 * - Right-click: Shows context menu (defined in IDR_TRAYPOPUP resource)
 * - Double-click: (handled by CTrayNotifyIcon - typically shows main window)
 * - Balloon notifications: Click events and timeouts
 */
LRESULT CMainFrame::OnTrayNotification(WPARAM wParam, LPARAM lParam)
{
	// Delegate all the work back to the default implementation in CTrayNotifyIcon
	// This includes showing popup menu, handling clicks, and managing balloon tips
	m_pTrayIcon.OnTrayNotification(wParam, lParam);
	return 0L;
}

/**
 * @brief Handles the File Print command
 */
void CMainFrame::OnFilePrint()
{
	// If already in print preview mode, trigger actual printing
	if (IsPrintPreview())
	{
		PostMessage(WM_COMMAND, AFX_ID_PREVIEW_PRINT);
	}
}

/**
 * @brief Handles the File Print Preview command
 */
void CMainFrame::OnFilePrintPreview()
{
	if (IsPrintPreview())
	{
		PostMessage(WM_COMMAND, AFX_ID_PREVIEW_CLOSE);  // force Print Preview mode closed
	}
}

/**
 * @brief Updates the UI state of the Print Preview command
 * @param pCmdUI Pointer to the command UI object
 */
void CMainFrame::OnUpdateFilePrintPreview(CCmdUI* pCmdUI)
{
	// Show checkmark next to menu item when in print preview mode
	pCmdUI->SetCheck(IsPrintPreview());
}

/**
 * @brief Shows the application window
 */
void CMainFrame::OnShowApplication()
{
	// The one and only window has been initialized, so show and update it
	ShowWindow(SW_SHOW);
	UpdateWindow();
}

/**
 * @brief Hides the application window
 */
void CMainFrame::OnHideApplication()
{
	// The one and only window has been initialized, so hide and update it
	ShowWindow(SW_HIDE);
	UpdateWindow();
}

/**
 * @brief Displays the Settings dialog and updates server configuration
 */
void CMainFrame::OnSettings()
{
	CSettingsDlg dlgSettings(this);
	if (dlgSettings.DoModal() == IDOK)
	{
		// Reload settings from registry (settings dialog saved them)
		// Note: Settings take effect on next connection attempt
		m_strServerIP = theApp.GetString(_T("ServerIP"), IntelliDiskIP);
		m_nServerPort = theApp.GetInt(_T("ServerPort"), IntelliDiskPort);
	}
}

/**
 * @brief Opens the IntelliDisk folder in Windows Explorer
 */
void CMainFrame::OnOpenFolder()
{
	// ShellExecute: verb="open", file=folder path, show=normal window
	ShellExecute(nullptr, _T("open"), GetSpecialFolder().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
}

/**
 * @brief Handles the View Online command
 */
void CMainFrame::OnViewOnline()
{
	// TODO: Add your command handler code here
}

/**
 * @brief Displays a message in the list control with the corresponding file icon
 * @param strMessage The message text to display
 * @param strFilePath The file path used to retrieve the file icon
 */
void CMainFrame::ShowMessage(const std::wstring& strMessage, const std::wstring& strFilePath)
{
	// Insert new item at the end of the list
	const int nListItem = m_wndView.GetListCtrl().InsertItem(m_wndView.GetListCtrl().GetItemCount(), strMessage.c_str());

	// Retrieve file type icon from system image list
	SHFILEINFO pshFileInfo = { 0 };
	SHGetFileInfo(
		strFilePath.c_str(),
		FILE_ATTRIBUTE_NORMAL,  // Use normal file attributes for generic icons
		&pshFileInfo, sizeof(pshFileInfo),
		SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);

	// Set the icon for this list item
	LVITEM lviItem = { 0 };
	lviItem.mask = LVIF_IMAGE;  // Only update the image
	lviItem.iItem = nListItem;
	lviItem.iSubItem = 0;  // First (and only) column
	lviItem.iImage = pshFileInfo.iIcon;  // System image list icon index
	VERIFY(m_wndView.GetListCtrl().SetItem(&lviItem));

	// Scroll to make the new item visible
	m_wndView.GetListCtrl().EnsureVisible(nListItem, FALSE);
}

/**
 * @brief Opens the developer's Twitter/X profile in the default browser
 */
void CMainFrame::OnTwitter()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://x.com/stefanmihaimoga"), nullptr, nullptr, SW_SHOW);
}

/**
 * @brief Opens the developer's LinkedIn profile in the default browser
 */
void CMainFrame::OnLinkedin()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://www.linkedin.com/in/stefanmihaimoga/"), nullptr, nullptr, SW_SHOW);
}

/**
 * @brief Opens the developer's Facebook profile in the default browser
 */
void CMainFrame::OnFacebook()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://www.facebook.com/stefanmihaimoga"), nullptr, nullptr, SW_SHOW);
}

/**
 * @brief Opens the developer's Instagram profile in the default browser
 */
void CMainFrame::OnInstagram()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://www.instagram.com/stefanmihaimoga/"), nullptr, nullptr, SW_SHOW);
}

/**
 * @brief Opens the IntelliDisk GitHub Issues page in the default browser
 */
void CMainFrame::OnIssues()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://github.com/mihaimoga/IntelliDisk/issues"), nullptr, nullptr, SW_SHOW);
}

/**
 * @brief Opens the IntelliDisk GitHub Discussions page in the default browser
 */
void CMainFrame::OnDiscussions()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://github.com/mihaimoga/IntelliDisk/discussions"), nullptr, nullptr, SW_SHOW);
}

/**
 * @brief Opens the IntelliDisk GitHub Wiki page in the default browser
 */
void CMainFrame::OnWiki()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://github.com/mihaimoga/IntelliDisk/wiki"), nullptr, nullptr, SW_SHOW);
}

/**
 * @brief Displays the User Manual in a web browser dialog
 */
void CMainFrame::OnUserManual()
{
	CWebBrowserDlg dlgWebBrowser(this);
	dlgWebBrowser.DoModal();
}

/**
 * @brief Displays the Check for Updates dialog
 */
void CMainFrame::OnCheckForUpdates()
{
	CCheckForUpdatesDlg dlgCheckForUpdates(this);
	dlgCheckForUpdates.DoModal();
}
