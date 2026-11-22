// NotifyDirCheck.cpp: implementation of the CNotifyDirCheck class.
//
//////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "NotifyDirCheck.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Simple Notification Callback
//////////////////////////////////////////////////////////////////////

UINT DefaultNotificationCallback(CFileInformation fiObject, EFileAction faAction, LPVOID lpData)
{
	UNREFERENCED_PARAMETER(lpData);
	CString csBuffer;
	CString csFile = fiObject.GetFilePath();

	if (IS_CREATE_FILE(faAction))
	{
		csBuffer.Format(_T("Created %s"), static_cast<LPCWSTR>(csFile));
		AfxMessageBox(csBuffer);
	}
	else if (IS_DELETE_FILE(faAction))
	{
		csBuffer.Format(_T("Deleted %s"), static_cast<LPCWSTR>(csFile));
		AfxMessageBox(csBuffer);
	}
	else if (IS_CHANGE_FILE(faAction))
	{
		csBuffer.Format(_T("Changed %s"), static_cast<LPCWSTR>(csFile));
		AfxMessageBox(csBuffer);
	}
	else
		return 1; //error, stop thread

	return 0; //success
}

//////////////////////////////////////////////////////////////////////
// Show Error Message Box
//////////////////////////////////////////////////////////////////////

static void ErrorMessage(CString failedSource)
{
	LPVOID  lpMsgBuf;
	CString csError;
	DWORD   dwLastError = GetLastError();//error number

	//make error comments
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		dwLastError,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),// Default language
		(LPTSTR)&lpMsgBuf,
		0,
		nullptr);

	csError.Format(_T("%s Error N%d\n%s"), static_cast<LPCWSTR>(failedSource), dwLastError, (LPCTSTR)lpMsgBuf);
	AfxMessageBox(csError, MB_OK | MB_ICONSTOP);

	// Free the buffer.
	LocalFree(lpMsgBuf);
}

//////////////////////////////////////////////////////////////////////
// Work Thread 
//////////////////////////////////////////////////////////////////////

UINT NotifyDirThread(LPVOID pParam)
{
	BOOL bStop = FALSE;

	HANDLE hDir = nullptr;

	CNotifyDirCheck *pNDC = (CNotifyDirCheck *)pParam;

	FI_List newFIL,
		oldFIL;

	FI_List fileList;

	EFileAction faAction;

	CFileInformation fi;


	if (pNDC == nullptr)
		return(0);

	hDir = FindFirstChangeNotification(pNDC->GetDirectory(),
		TRUE,
		FILE_NOTIFY_CHANGE_FILE_NAME |
		FILE_NOTIFY_CHANGE_DIR_NAME |
		FILE_NOTIFY_CHANGE_SIZE |
		FILE_NOTIFY_CHANGE_LAST_WRITE |
		FILE_NOTIFY_CHANGE_ATTRIBUTES
	);

	if (hDir == INVALID_HANDLE_VALUE)
	{
		ErrorMessage(_T("FindFirstChangeNotification"));
		return(0);
	}

	while (pNDC->IsRun())
	{
		CFileInformation::RemoveFiles(&oldFIL);
		CFileInformation::EnumFiles(pNDC->GetDirectory(), &oldFIL);

		bStop = FALSE;

		while (WaitForSingleObject(hDir, WAIT_TIMEOUT) != WAIT_OBJECT_0)
		{
			if (!pNDC->IsRun())
			{
				bStop = TRUE;//to end
				break;
			}
		}

		if (bStop)
			break;//to end

		Sleep(WAIT_TIMEOUT);

		CFileInformation::RemoveFiles(&newFIL);
		CFileInformation::EnumFiles(pNDC->GetDirectory(), &newFIL);

		// Sleep( WAIT_TIMEOUT);

		// faAction = CFileInformation::CompareFiles( &oldFIL, &newFIL, fi);
		faAction = CFileInformation::CompareFiles(&oldFIL, &newFIL, &fileList);

		if (!IS_NOTACT_FILE(faAction))
		{
			NOTIFICATION_CALLBACK_PTR ncpAction = pNDC->GetActionCallback();

			POSITION fileListPos = nullptr;

			fileListPos = fileList.GetHeadPosition();

			while (fileListPos)
			{
				fi = fileList.GetAt(fileListPos);

				if (ncpAction) //call user's callback
					bStop = (ncpAction(fi, faAction, pNDC->GetData()) > 0);

				else //call user's virtual function
					bStop = (pNDC->Action(fi, faAction) > 0);

				if (bStop)
					break;//to end

				fileList.GetNext(fileListPos);
			}
		}

		if (FindNextChangeNotification(hDir) == 0)
		{
			ErrorMessage(_T("FindNextChangeNotification"));
			return(0);
		}
	}

	//end point of notification thread
	CFileInformation::RemoveFiles(&newFIL);
	CFileInformation::RemoveFiles(&oldFIL);

	return FindCloseChangeNotification(hDir);
}

//////////////////////////////////////////////////////////////////////
// Class 
//////////////////////////////////////////////////////////////////////

CNotifyDirCheck::CNotifyDirCheck()
{
	SetDirectory(_T(""));
	SetActionCallback(nullptr);
	SetData(nullptr);
	SetStop();
	m_pThread = nullptr;
}

CNotifyDirCheck::CNotifyDirCheck(CString csDir, NOTIFICATION_CALLBACK_PTR ncpAction, LPVOID lpData)
{
	SetDirectory(csDir);
	SetActionCallback(ncpAction);
	SetData(lpData);
	SetStop();
	m_pThread = nullptr;
}

CNotifyDirCheck::~CNotifyDirCheck()
{
	Stop();
}

BOOL CNotifyDirCheck::Run()
{
	if (IsRun() || m_pThread != nullptr || m_csDir.IsEmpty())
		return FALSE;

	SetRun();
	m_pThread = AfxBeginThread(NotifyDirThread, this);

	if (m_pThread == nullptr)
		SetStop();

	return IsRun();
}

void CNotifyDirCheck::Stop()
{
	if (!IsRun() || m_pThread == nullptr)
		return;

	SetStop();

	WaitForSingleObject(m_pThread->m_hThread, 2 * NOTIFICATION_TIMEOUT);
	m_pThread = nullptr;
}

UINT CNotifyDirCheck::Action(CFileInformation fiObject, EFileAction faAction)
{
	CString csBuffer;
	CString csFile = fiObject.GetFilePath();

	if (IS_CREATE_FILE(faAction))
	{
		csBuffer.Format(_T("Created %s"), static_cast<LPCWSTR>(csFile));
		AfxMessageBox(csBuffer);
	}
	else if (IS_DELETE_FILE(faAction))
	{
		csBuffer.Format(_T("Deleted %s"), static_cast<LPCWSTR>(csFile));
		AfxMessageBox(csBuffer);
	}
	else if (IS_CHANGE_FILE(faAction))
	{
		csBuffer.Format(_T("Changed %s"), static_cast<LPCWSTR>(csFile));
		AfxMessageBox(csBuffer);
	}
	else
		return 1; //error, stop thread

	return 0; //success
}
