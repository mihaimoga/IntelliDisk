/* This file is part of IntelliDisk application developed by Stefan-Mihai MOGA.

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
END_MESSAGE_MAP()

// CMainFrame construction/destruction

CMainFrame::CMainFrame() noexcept
{
	m_hMainFrameIcon = CTrayNotifyIcon::LoadIcon(IDR_MAINFRAME);
	m_MainButton = nullptr;
	m_hOccupiedSemaphore = CreateSemaphore(nullptr, 0, NOTIFY_FILE_SIZE, nullptr);
	m_hEmptySemaphore = CreateSemaphore(nullptr, NOTIFY_FILE_SIZE, NOTIFY_FILE_SIZE, nullptr);
	m_hResourceMutex = CreateSemaphore(nullptr, 1, 1, nullptr);
	m_hSocketMutex = CreateSemaphore(nullptr, 1, 1, nullptr);
	m_nNextIn = m_nNextOut = 0;
}

CMainFrame::~CMainFrame()
{
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

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
		return -1;

	// create a view to occupy the client area of the frame
	if (!m_wndView.Create(nullptr, nullptr, AFX_WS_DEFAULT_VIEW, CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, nullptr))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}

	m_wndRibbonBar.Create(this);
	m_wndRibbonBar.LoadFromResource(IDR_RIBBON);

	m_MainButton = new CMFCRibbonApplicationButton;
	m_MainButton->SetVisible(FALSE);
	m_wndRibbonBar.SetApplicationButton(m_MainButton, CSize());

	// enable Visual Studio 2005 style docking window behavior
	CDockingManager::SetDockingMode(DT_SMART);
	// enable Visual Studio 2005 style docking window auto-hide behavior
	EnableAutoHidePanes(CBRS_ALIGN_ANY);

	if (!m_pTrayIcon.Create(this, IDR_TRAYPOPUP, _T("IntelliDisk"), m_hMainFrameIcon, WM_TRAYNOTIFY))
	{
		AfxMessageBox(_T("Failed to create tray icon"), MB_OK | MB_ICONSTOP);
		return -1;
	}

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
		VERIFY(m_pImageList.Attach(hSystemImageList));
		m_wndView.GetListCtrl().SetImageList(&m_pImageList, LVSIL_SMALL);
	}

	CRect rectClient;
	m_wndView.GetListCtrl().GetClientRect(&rectClient);
	m_wndView.GetListCtrl().InsertColumn(0, _T("Operation"), LVCFMT_LEFT, rectClient.Width());

	m_pNotifyDirCheck.SetDirectory(GetSpecialFolder().c_str());
	m_pNotifyDirCheck.SetData(this);
	// set your callback to work with each new event
	m_pNotifyDirCheck.SetActionCallback(DirCallback);
	m_pNotifyDirCheck.Run();

	m_strServerIP = theApp.GetString(_T("ServerIP"), IntelliDiskIP);
	m_nServerPort = theApp.GetInt(_T("ServerPort"), IntelliDiskPort);

	m_hProducerThread = CreateThread(nullptr, 0, ProducerThread, this, 0, &m_dwThreadID[0]);
	ASSERT(m_hProducerThread != nullptr);
	m_hConsumerThread = CreateThread(nullptr, 0, ConsumerThread, this, 0, &m_dwThreadID[1]);
	ASSERT(m_hConsumerThread != nullptr);

	return 0;
}

void CMainFrame::OnDestroy()
{
	m_pNotifyDirCheck.Stop();

	AddNewItem(ID_STOP_PROCESS, std::wstring(_T("")), this);

	HANDLE hThreadArray[2] = { 0, 0 };
	hThreadArray[0] = m_hProducerThread;
	hThreadArray[1] = m_hConsumerThread;
	WaitForMultipleObjects(2, hThreadArray, TRUE, INFINITE);

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

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if (!CFrameWndEx::PreCreateWindow(cs))
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.lpszClass = AfxRegisterWndClass(0);
	return TRUE;
}

// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWndEx::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWndEx::Dump(dc);
}
#endif //_DEBUG

// CMainFrame message handlers

void CMainFrame::OnSetFocus(CWnd* /*pOldWnd*/)
{
	// forward focus to the view window
	m_wndView.SetFocus();
}

BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// let the view have first crack at the command
	if (m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return TRUE;

	// otherwise, do default handling
	return CFrameWndEx::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

LRESULT CMainFrame::OnTrayNotification(WPARAM wParam, LPARAM lParam)
{
	//Delegate all the work back to the default implementation in CTrayNotifyIcon.
	m_pTrayIcon.OnTrayNotification(wParam, lParam);
	return 0L;
}

void CMainFrame::OnFilePrint()
{
	if (IsPrintPreview())
	{
		PostMessage(WM_COMMAND, AFX_ID_PREVIEW_PRINT);
	}
}

void CMainFrame::OnFilePrintPreview()
{
	if (IsPrintPreview())
	{
		PostMessage(WM_COMMAND, AFX_ID_PREVIEW_CLOSE);  // force Print Preview mode closed
	}
}

void CMainFrame::OnUpdateFilePrintPreview(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(IsPrintPreview());
}

void CMainFrame::OnShowApplication()
{
	// The one and only window has been initialized, so show and update it
	ShowWindow(SW_SHOW);
	UpdateWindow();
}

void CMainFrame::OnHideApplication()
{
	// The one and only window has been initialized, so hide and update it
	ShowWindow(SW_HIDE);
	UpdateWindow();
}

void CMainFrame::OnSettings()
{
	CSettingsDlg dlgSettings(this);
	if (dlgSettings.DoModal() == IDOK)
	{
		m_strServerIP = theApp.GetString(_T("ServerIP"), IntelliDiskIP);
		m_nServerPort = theApp.GetInt(_T("ServerPort"), IntelliDiskPort);
	}
}

void CMainFrame::OnOpenFolder()
{
	ShellExecute(nullptr, _T("open"), GetSpecialFolder().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
}

void CMainFrame::OnViewOnline()
{
	// TODO: Add your command handler code here
}

void CMainFrame::ShowMessage(const std::wstring& strMessage, const std::wstring& strFilePath)
{
	const int nListItem = m_wndView.GetListCtrl().InsertItem(m_wndView.GetListCtrl().GetItemCount(), strMessage.c_str());
	SHFILEINFO pshFileInfo = { 0 };
	SHGetFileInfo(
		strFilePath.c_str(),
		FILE_ATTRIBUTE_NORMAL,
		&pshFileInfo, sizeof(pshFileInfo),
		SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
	LVITEM lviItem = { 0 };
	lviItem.mask = LVIF_IMAGE;
	lviItem.iItem = nListItem;
	lviItem.iSubItem = 0;
	lviItem.iImage = pshFileInfo.iIcon;
	VERIFY(m_wndView.GetListCtrl().SetItem(&lviItem));
	m_wndView.GetListCtrl().EnsureVisible(nListItem, FALSE);
}

void CMainFrame::OnTwitter()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://twitter.com/stefanmihaimoga"), nullptr, nullptr, SW_SHOW);
}

void CMainFrame::OnLinkedin()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://www.linkedin.com/in/stefanmihaimoga/"), nullptr, nullptr, SW_SHOW);
}

void CMainFrame::OnFacebook()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://www.facebook.com/stefanmihaimoga"), nullptr, nullptr, SW_SHOW);
}

void CMainFrame::OnInstagram()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://www.instagram.com/stefanmihaimoga/"), nullptr, nullptr, SW_SHOW);
}

void CMainFrame::OnIssues()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://github.com/mihaimoga/IntelliDisk/issues"), nullptr, nullptr, SW_SHOW);
}

void CMainFrame::OnDiscussions()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://github.com/mihaimoga/IntelliDisk/discussions"), nullptr, nullptr, SW_SHOW);
}

void CMainFrame::OnWiki()
{
	::ShellExecute(GetSafeHwnd(), _T("open"), _T("https://github.com/mihaimoga/IntelliDisk/wiki"), nullptr, nullptr, SW_SHOW);
}
