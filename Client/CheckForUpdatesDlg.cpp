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

// CheckForUpdatesDlg.cpp : implementation file
//

#include "pch.h"
#include "IntelliDisk.h"
#include "CheckForUpdatesDlg.h"

// Include genUp4win update checking library
#include "../genUp4win/genUp4win.h"

// Link appropriate genUp4win library based on architecture and configuration
// This ensures we link the correct build variant (x86/x64, Debug/Release)
#if _WIN64
#ifdef _DEBUG
#pragma comment(lib, "../Setup/x64/Debug/genUp4win.lib")    // 64-bit Debug
#else
#pragma comment(lib, "../Setup/x64/Release/genUp4win.lib")  // 64-bit Release
#endif
#else
#ifdef _DEBUG
#pragma comment(lib, "../Setup/Debug/genUp4win.lib")        // 32-bit Debug
#else
#pragma comment(lib, "../Setup/Release/genUp4win.lib")      // 32-bit Release
#endif
#endif

// CCheckForUpdatesDlg dialog

IMPLEMENT_DYNAMIC(CCheckForUpdatesDlg, CDialogEx)

/**
 * @brief Constructor for the Check For Updates dialog
 * @param pParent Pointer to the parent window
 */
CCheckForUpdatesDlg::CCheckForUpdatesDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CheckForUpdatesDlg, pParent)
{
	// Initialize thread management variables
	m_nUpdateThreadID = 0;     // Thread ID (for identification)
	m_hUpdateThread = nullptr; // Thread handle (for synchronization)
	m_nTimerID = 0;            // Timer ID (for polling thread status)
}

/**
 * @brief Destructor for the Check For Updates dialog
 */
CCheckForUpdatesDlg::~CCheckForUpdatesDlg()
{
}

/**
 * @brief Exchanges data between dialog controls and member variables
 * @param pDX Pointer to a CDataExchange object
 */
void CCheckForUpdatesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATUS, m_ctrlStatusMessage);
	DDX_Control(pDX, IDC_PROGRESS, m_ctrlProgress);
}

BEGIN_MESSAGE_MAP(CCheckForUpdatesDlg, CDialogEx)
	ON_WM_TIMER()
END_MESSAGE_MAP()

// CCheckForUpdatesDlg message handlers

// === GLOBAL STATE FOR UPDATE THREAD ===
// Global pointer to allow callback function to access dialog controls
// Required because CheckForUpdates() callback doesn't support user data parameter
CCheckForUpdatesDlg* g_dlgCheckForUpdates = nullptr;

/**
 * @brief UI callback function for update progress messages
 * @param (unnamed) Integer parameter (unused)
 * @param strMessage Status message to display in the UI
 * 
 * ARCHITECTURE NOTE:
 * =================
 * This callback is invoked by the genUp4win library during update checks
 * to provide progress feedback. Since it cannot pass user data, we use
 * a global pointer (g_dlgCheckForUpdates) to access the dialog.
 */
void UI_Callback(int, const std::wstring& strMessage)
{
	if (g_dlgCheckForUpdates != nullptr)
	{
		// Update status message in dialog
		g_dlgCheckForUpdates->m_ctrlStatusMessage.SetWindowText(strMessage.c_str());
		// Force immediate UI update (don't wait for message queue)
			g_dlgCheckForUpdates->m_ctrlStatusMessage.UpdateWindow();
			}
		}

		// Thread synchronization flags (accessed by both UI and worker threads)
		bool g_bThreadRunning = false;    // Set to true while update check is in progress
		bool g_bNewUpdateFound = false;   // Set to true if a newer version is available

/**
 * @brief Thread procedure for checking for updates
 * @param lpParam Pointer to thread parameters (unused)
 * @return Thread exit code
 * 
 * WORKFLOW:
 * =========
 * 1. Set thread running flag
 * 2. Show marquee progress animation
 * 3. Get path to current executable
 * 4. Call CheckForUpdates() with APPLICATION_URL and UI callback
 * 5. Hide progress animation
 * 6. Store result (new update found or not)
 * 7. Clear thread running flag
 */
DWORD WINAPI UpdateThreadProc(LPVOID lpParam)
{
	UNREFERENCED_PARAMETER(lpParam);

	// Signal that update check is in progress
	g_bThreadRunning = true;

	// Show animated progress bar
	if (g_dlgCheckForUpdates != nullptr)
	{
		// Marquee mode: continuous animation, 30ms update interval
		g_dlgCheckForUpdates->m_ctrlProgress.SetMarquee(TRUE, 30);
	}

	// Get full path to current executable for version comparison
	const DWORD nLength = 0x1000 /* _MAX_PATH */;
	TCHAR lpszFilePath[nLength] = { 0, };
	GetModuleFileName(nullptr, lpszFilePath, nLength);

	// Check for updates using genUp4win library
	// APPLICATION_URL contains the version info URL
	// UI_Callback provides progress feedback to user
	g_bNewUpdateFound = CheckForUpdates(lpszFilePath, APPLICATION_URL, UI_Callback);

	// Hide progress bar animation
	if (g_dlgCheckForUpdates != nullptr)
	{
		g_dlgCheckForUpdates->m_ctrlProgress.SetMarquee(FALSE, 30);
	}

	// Signal that update check is complete
	g_bThreadRunning = false;

	// Exit thread (ExitThread preferred over return for explicit cleanup)
	::ExitThread(0);
	// return 0;
}

/**
 * @brief Initializes the dialog when it is first created
 * @return TRUE to set focus to the first control, FALSE otherwise
 */
BOOL CCheckForUpdatesDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

#ifdef _DEBUG
	// DEBUG ONLY: Write a test configuration file for genUp4win
	// This creates a .ini file next to the executable with update URL
	// In production, this is pre-configured during build/installation
	const DWORD nLength = 0x1000 /* _MAX_PATH */;
	TCHAR lpszFilePath[nLength] = { 0, };
	GetModuleFileName(nullptr, lpszFilePath, nLength);
	// INSTALLER_URL points to the update package location
	WriteConfigFile(lpszFilePath, INSTALLER_URL);
#endif

	// Set global pointer for callback access
	g_dlgCheckForUpdates = this;

	// Create worker thread to check for updates asynchronously
	// This prevents blocking the UI during network operations
	m_hUpdateThread = ::CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)UpdateThreadProc, this, 0, &m_nUpdateThreadID);

	// Start timer to poll thread status (100ms intervals)
	// We use polling instead of WaitForSingleObject to keep UI responsive
	m_nTimerID = SetTimer(0x1234, 100, nullptr);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

/**
 * @brief Handles the cancel button or close action
 */
void CCheckForUpdatesDlg::OnCancel()
{
	// Wait for update thread to complete before closing dialog
	// This prevents closing while network operations are in progress
	while (g_bThreadRunning)
		Sleep(1000);  // Poll every second until thread exits

	CDialogEx::OnCancel();
}

/**
 * @brief Handles timer events for monitoring update thread status
 * @param nIDEvent The identifier of the timer that expired
 * 
 * TIMER-BASED THREAD MONITORING:
 * ===============================
 * Instead of blocking on WaitForSingleObject(), we use a timer to
 * periodically check if the update thread has finished. This keeps
 * the UI responsive and allows the user to see progress updates.
 */
void CCheckForUpdatesDlg::OnTimer(UINT_PTR nIDEvent)
{
	CDialogEx::OnTimer(nIDEvent);

	// Check if this is our thread monitoring timer
	if (m_nTimerID == nIDEvent)
	{
		// Check if update thread has completed
		if (!g_bThreadRunning)
		{
			// Thread finished - stop timer and close dialog
			VERIFY(KillTimer(m_nTimerID));
			CDialogEx::OnCancel();

			// If a new update was found, close the entire application
			// The genUp4win library will have already prompted the user
			// to download and install the update
			if (g_bNewUpdateFound)
			{
				PostQuitMessage(0);  // Exit application message loop
			}
		}
	}
}
