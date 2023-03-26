// NotifyDirCheck.h: interface for the CNotifyDirCheck class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NOTIFYDIRCHECK_H__44DFE393_51AF_42C2_BA07_A1628BDA25FC__INCLUDED_)
#define AFX_NOTIFYDIRCHECK_H__44DFE393_51AF_42C2_BA07_A1628BDA25FC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
#error include 'stdafx.h' before including this file for PCH
#endif

#include "FileInformation.h"	// for file information class

typedef UINT NOTIFICATION_CALLBACK(CFileInformation fiObject, EFileAction faAction, LPVOID lpData);
typedef NOTIFICATION_CALLBACK* NOTIFICATION_CALLBACK_PTR;

UINT NotifyDirThread(LPVOID pParam);

#define NOTIFICATION_TIMEOUT 1000

class CNotifyDirCheck : public CObject
{
public:
	CNotifyDirCheck();
	CNotifyDirCheck(CString csDir, NOTIFICATION_CALLBACK_PTR ncpAction, LPVOID lpData = nullptr);
	virtual ~CNotifyDirCheck();

	const NOTIFICATION_CALLBACK_PTR GetActionCallback() const { return m_ncpAction; }
	void                            SetActionCallback(NOTIFICATION_CALLBACK_PTR ncpAction) { m_ncpAction = ncpAction; }
	CString                         GetDirectory() const { return m_csDir; }
	void                            SetDirectory(CString csDir) { m_csDir = csDir; }
	LPVOID                          GetData() const { return m_lpData; }
	void                            SetData(LPVOID lpData) { m_lpData = lpData; }
	BOOL                            IsRun() const { return m_isRun; }
	BOOL                            Run();
	void                            Stop();

	//Override Action to work with each new event  
	virtual UINT                    Action(CFileInformation fiObject, EFileAction faAction);

protected:
	void                            SetRun() { m_isRun = TRUE; }
	void                            SetStop() { m_isRun = FALSE; }

protected:
	NOTIFICATION_CALLBACK_PTR m_ncpAction;
	CWinThread*               m_pThread;
	CString                   m_csDir;
	BOOL                      m_isRun;
	LPVOID					  m_lpData;
};

#endif // !defined(AFX_NOTIFYDIRCHECK_H__44DFE393_51AF_42C2_BA07_A1628BDA25FC__INCLUDED_)
