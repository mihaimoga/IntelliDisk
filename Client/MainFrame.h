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

// MainFrame.h : interface of the CMainFrame class
//

#pragma once

#include "ChildView.h"
#include "NTray.h"
#include "FileInformation.h"
#include "NotifyDirCheck.h"
#include "SocMFC.h"

constexpr auto BSIZE = 0x10000; // this is only for testing, not for the final commercial application
constexpr auto NOTIFY_FILE_SIZE = 0x10000; // this is only for testing, not for the final commercial application

// Structure to hold file event information in the notification queue
typedef struct {
	int nFileEvent;          // Event type (stop, download, upload, delete)
	std::wstring strFilePath; // Full path to the file being processed
} NOTIFY_FILE_DATA;

// File event identifiers for queue processing
#define ID_STOP_PROCESS 0x01   // Stop processing and shutdown threads
#define ID_FILE_DOWNLOAD 0x02  // Download file from server
#define ID_FILE_UPLOAD 0x03    // Upload file to server
#define ID_FILE_DELETE 0x04    // Delete file on server

class CMainFrame : public CFrameWndEx
{
	
public:
	CMainFrame() noexcept;
protected: 
	DECLARE_DYNAMIC(CMainFrame)

// Attributes
public:
	HICON m_hMainFrameIcon;
	CTrayNotifyIcon m_pTrayIcon;
	CNotifyDirCheck m_pNotifyDirCheck;
	CImageList m_pImageList;
	HANDLE m_hOccupiedSemaphore;
	HANDLE m_hEmptySemaphore;
	HANDLE m_hResourceMutex;
	HANDLE m_hSocketMutex;
	int m_nNextIn;
	int m_nNextOut;
	NOTIFY_FILE_DATA m_pResourceArray[NOTIFY_FILE_SIZE] = { 0, };
	HANDLE m_hProducerThread = nullptr;
	HANDLE m_hConsumerThread = nullptr;
	DWORD m_dwThreadID[2] = { 0, };
	CWSocket m_pApplicationSocket;
	CString m_strServerIP;
	int m_nServerPort = 0;

// Operations
public:

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:  // control bar embedded members
	CMFCRibbonBar     m_wndRibbonBar;
	CMFCRibbonApplicationButton* m_MainButton;
	CMFCToolBarImages m_PanelImages;
	CMFCRibbonStatusBar  m_wndStatusBar;
	CChildView    m_wndView;

// Generated message map functions
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	afx_msg void OnFilePrint();
	afx_msg void OnFilePrintPreview();
	afx_msg void OnUpdateFilePrintPreview(CCmdUI* pCmdUI);
	afx_msg LRESULT OnTrayNotification(WPARAM wParam, LPARAM lParam);
	afx_msg void OnShowApplication();
	afx_msg void OnHideApplication();
	afx_msg void OnSettings();
	afx_msg void OnOpenFolder();
	afx_msg void OnViewOnline();
public:
	afx_msg void ShowMessage(const std::wstring& strMessage, const std::wstring& strFilePath);
protected:
	afx_msg void OnTwitter();
	afx_msg void OnLinkedin();
	afx_msg void OnFacebook();
	afx_msg void OnInstagram();
	afx_msg void OnIssues();
	afx_msg void OnDiscussions();
	afx_msg void OnWiki();
	afx_msg void OnUserManual();
	afx_msg void OnCheckForUpdates();

	DECLARE_MESSAGE_MAP()
};
