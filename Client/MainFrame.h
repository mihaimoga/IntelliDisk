/* This file is part of IntelliDisk application developed by Stefan-Mihai MOGA.

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

typedef struct {
	int nFileEvent;
	std::wstring strFilePath;
} NOTIFY_FILE_DATA;

#define ID_STOP_PROCESS 0x01
#define ID_FILE_DOWNLOAD 0x02
#define ID_FILE_UPLOAD 0x03
#define ID_FILE_DELETE 0x04

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
	NOTIFY_FILE_DATA m_pResourceArray[BSIZE];
	HANDLE m_hProducerThread;
	HANDLE m_hConsumerThread;
	DWORD m_dwThreadID[2];
	CWSocket m_pApplicationSocket;
	CString m_strServerIP;
	int m_nServerPort;

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

	DECLARE_MESSAGE_MAP()
};
