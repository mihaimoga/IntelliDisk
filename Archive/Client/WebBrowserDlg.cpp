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

// WebBrowserDlg.cpp : implementation file
//

#include "pch.h"
#include "IntelliDisk.h"
#include "WebBrowserDlg.h"

// CWebBrowserDlg dialog

IMPLEMENT_DYNAMIC(CWebBrowserDlg, CDialogEx)

/**
 * @brief Constructor for the Web Browser dialog
 * @param pParent Pointer to the parent window
 * @details This dialog hosts an embedded web browser control (based on Edge WebView2)
 *          to display the IntelliDisk user manual
 */
CWebBrowserDlg::CWebBrowserDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_WebBrowserDlg, pParent)
{
}

/**
 * @brief Destructor for the Web Browser dialog
 */
CWebBrowserDlg::~CWebBrowserDlg()
{
}

/**
 * @brief Exchanges data between dialog controls and member variables
 * @param pDX Pointer to a CDataExchange object
 */
void CWebBrowserDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CWebBrowserDlg, CDialogEx)
	ON_WM_DESTROY()
    ON_WM_SIZE()
END_MESSAGE_MAP()

// CWebBrowserDlg message handlers

/**
 * @brief Initializes the Web Browser dialog when it is first created
 * @details Creates and configures an embedded web browser control asynchronously.
 *          The browser is based on Microsoft Edge WebView2 for modern web standards support.
 * @return TRUE to set focus to the first control, FALSE otherwise
 * 
 * INITIALIZATION SEQUENCE:
 * ========================
 * 1. Create web browser control asynchronously (waits for WebView2 runtime)
 * 2. Configure browser: disable popups, set parent view
 * 3. Navigate to USER_MANUAL_URL (local or remote HTML documentation)
 * 4. Register callback to sync window title with page title
 * 
 * ASYNC PATTERN:
 * ==============
 * WebView2 initialization is asynchronous because it requires:
 * - WebView2 runtime download/installation (if not present)
 * - Process creation for browser engine
 * - IPC channel establishment between host and browser process
 * The completion callback ensures browser is ready before navigation.
 */
BOOL CWebBrowserDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Get client rectangle to size browser control to fill entire dialog
	CRect rectClient;
	GetClientRect(rectClient);

	// === CREATE WEB BROWSER CONTROL ASYNCHRONOUSLY ===
	// CreateAsync returns immediately; browser initialization happens in background
	m_pCustomControl.CreateAsync(
		WS_VISIBLE | WS_CHILD,  // Standard visible child window
		rectClient,             // Initial size (fills dialog)
		this,                   // Parent window
		1,                      // Control ID
		[this]() {              // Completion callback (invoked when WebView2 is ready)
			// Set parent view for proper MFC integration
			m_pCustomControl.SetParentView((CView*)this);

			// Disable popup windows (security/UX best practice)
			m_pCustomControl.DisablePopups();

			// Navigate to user manual URL (can be local file:// or https://)
			m_pCustomControl.Navigate(USER_MANUAL_URL, nullptr);

			// Register callback to update dialog title when page title changes
			m_pCustomControl.RegisterCallback(CWebBrowser::CallbackType::TitleChanged, [this]() {
				CString strTitle = m_pCustomControl.GetTitle();
				SetWindowText(strTitle);  // Sync dialog title with page title
				});
		});

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

/**
 * @brief Handles cleanup when the dialog is destroyed
 * @details Explicitly destroys the web browser control to ensure proper cleanup
 *          of WebView2 resources (browser process, IPC channels, memory)
 */
void CWebBrowserDlg::OnDestroy()
{
	// Destroy web browser control before dialog destruction
	// This ensures orderly shutdown of browser process and resources
	VERIFY(m_pCustomControl.DestroyWindow());
	
    CDialogEx::OnDestroy();
}

/**
 * @brief Handles dialog resize events
 * @param nType Type of resizing requested (SIZE_MAXIMIZED, SIZE_MINIMIZED, etc.)
 * @param cx New width of the client area
 * @param cy New height of the client area
 * @details Resizes the web browser control to fill the entire dialog client area,
 *          ensuring the user manual remains readable at any window size
 */
void CWebBrowserDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);

    // Resize browser control to fill entire dialog (if already created)
    if (m_pCustomControl.GetSafeHwnd() != nullptr)
    {
        // Position browser at (0,0) and size to match dialog client area
        m_pCustomControl.MoveWindow(0, 0, cx, cy);
    }
}
