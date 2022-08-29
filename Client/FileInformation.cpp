// FileInformation.cpp: implementation of the CFileInformation class.
//
//////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "FileInformation.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFileInformation::CFileInformation()
{
	ZeroFileData();
	SetFileDir(CString(_T("default.txt")));
}

CFileInformation::CFileInformation(CString path)
{
	Load(path);
}

CFileInformation::CFileInformation(CString dir, CString file)
{
	Load(dir, file);
}

CFileInformation::CFileInformation(WIN32_FIND_DATA fd, CString dir)
{
	SetFileData(fd);
	SetFileDir(dir);
}

CFileInformation::CFileInformation(const CFileInformation& fdi)
{
	SetFileData(fdi.GetFileData());
	SetFileDir(fdi.GetFileDir());
}

CFileInformation::CFileInformation(const CFileInformation* fdi)
{
	SetFileData(fdi->GetFileData());
	SetFileDir(fdi->GetFileDir());
}

CFileInformation::~CFileInformation()
{
}

void CFileInformation::SetFileData(WIN32_FIND_DATA fd)
{
	memcpy(&m_fd, &fd, sizeof(WIN32_FIND_DATA));
}

WIN32_FIND_DATA CFileInformation::GetFileData() const
{
	return m_fd;
}

void CFileInformation::ZeroFileData()
{
	memset(&m_fd, 0, sizeof(WIN32_FIND_DATA));
}

CString CFileInformation::GetFileName() const
{
	CString buffer(m_fd.cFileName);
	return buffer;
}

CString CFileInformation::GetFileNameWithoutExt() const
{
	CString buffer = GetFileName();
	int     pos = buffer.ReverseFind(L'.');
	return  buffer.Left(pos);
}

CString CFileInformation::GetFileExt() const
{
	CString buffer = GetFileName();
	int     pos = buffer.ReverseFind(L'.');
	return  buffer.Right(buffer.GetLength() - pos - 1);
}

unsigned long CFileInformation::GetFileSize(DWORD &dwFileSizeHigh, DWORD &dwFileSizeLow) const
{
	dwFileSizeHigh = m_fd.nFileSizeHigh;
	dwFileSizeLow = m_fd.nFileSizeLow;
	return dwFileSizeLow;
}

unsigned long CFileInformation::GetFileSize(EFileSize& fsFormat) const
{
	DWORD         dwFileSizeHigh = 0;
	DWORD         dwFileSizeLow = 0;
	unsigned long size = 0;

	GetFileSize(dwFileSizeHigh, dwFileSizeLow);

	if (dwFileSizeLow == 0)
	{
		fsFormat = fsBytes;
		return dwFileSizeLow;
	}

	if (dwFileSizeHigh == 0)
	{
		size = dwFileSizeLow - (dwFileSizeLow % 1024);

		if (size == 0)
		{
			fsFormat = fsBytes;
			return dwFileSizeLow;
		}
		else
		{
			fsFormat = fsKBytes;
			return (size / 1024);
		}
	}
	else
	{
		fsFormat = fsKBytes;
		return dwFileSizeHigh * (MAXDWORD / 1024) + dwFileSizeLow / 1024;
	}
}

DWORD CFileInformation::GetFileAttribute() const
{
	return m_fd.dwFileAttributes;
}

BOOL CFileInformation::IsDirectory() const
{
	return ((GetFileAttribute() != -1) &&
		(GetFileAttribute() &  FILE_ATTRIBUTE_DIRECTORY));
}

BOOL CFileInformation::IsArchive() const
{
	return ((GetFileAttribute() != -1) &&
		(GetFileAttribute() &  FILE_ATTRIBUTE_ARCHIVE));
}

BOOL CFileInformation::IsNormal() const
{
	return ((GetFileAttribute() != -1) &&
		(GetFileAttribute() &  FILE_ATTRIBUTE_NORMAL));
}

BOOL CFileInformation::IsHidden() const
{
	return ((GetFileAttribute() != -1) &&
		(GetFileAttribute() &  FILE_ATTRIBUTE_HIDDEN));
}

BOOL CFileInformation::IsReadOnly() const
{
	return ((GetFileAttribute() != -1) &&
		(GetFileAttribute() &  FILE_ATTRIBUTE_READONLY));
}

BOOL CFileInformation::FileAttributeReadOnly(BOOL set)
{
	if (GetFileAttribute() == -1)
		return FALSE;

	if (set)
	{
		return SetFileAttributes(GetFilePath(), GetFileAttribute() | FILE_ATTRIBUTE_READONLY);
	}
	else
	{
		return SetFileAttributes(GetFilePath(), GetFileAttribute()&~FILE_ATTRIBUTE_READONLY);
	}
}

BOOL CFileInformation::IsSystem() const
{
	return ((GetFileAttribute() != -1) &&
		(GetFileAttribute() &  FILE_ATTRIBUTE_SYSTEM));
}

BOOL CFileInformation::IsTemporary() const
{
	return ((GetFileAttribute() != -1) &&
		(GetFileAttribute() &  FILE_ATTRIBUTE_TEMPORARY));
}

BOOL CFileInformation::IsWinTemporary() const
{
	if ((GetFileAttribute() != -1) &&
		(GetFileAttribute() & FILE_ATTRIBUTE_TEMPORARY))
		return TRUE;

	CString fileName = GetFileName();
	fileName.MakeLower();
	if ((fileName[0] == L'~') ||
		(fileName.Find(_T(".tmp"), 0) != -1) ||
		(fileName.Find(_T(".bak"), 0) != -1))
		return TRUE;

	return FALSE;
}

BOOL CFileInformation::IsNotAvailableNow() const
{
	return ((GetFileAttribute() != -1) &&
		(GetFileAttribute() &  FILE_ATTRIBUTE_OFFLINE));
}

BOOL CFileInformation::IsCurrentRoot() const
{
	return (GetFileName().Compare(_T(".")) == 0);
}

BOOL CFileInformation::IsParentDir() const
{
	return (GetFileName().Compare(_T("..")) == 0);
}

BOOL CFileInformation::IsRootFile() const
{
	return IsCurrentRoot() || IsParentDir();
}

BOOL CFileInformation::IsActualFile() const
{
	return !IsDirectory() &&
		!IsCurrentRoot() &&
		!IsParentDir() &&
		!IsWinTemporary() &&
		!IsHidden() &&
		!IsSystem();
}

CString CFileInformation::GetFileDir() const
{
	return m_dir;
}

void CFileInformation::SetFileDir(const CString &dir)
{
	m_dir = dir;
}

CString CFileInformation::GetFilePath() const
{
	CString path;

	if (m_dir.IsEmpty() || m_dir[m_dir.GetLength() - 1] == L'\\')
		path = m_dir + GetFileName();
	else
		path = m_dir + _T("\\") + GetFileName();

	return path;
}

FILETIME CFileInformation::GetFileLastWriteTime() const
{
	return m_fd.ftLastWriteTime;
}

BOOL CFileInformation::operator ==(CFileInformation fdi) const
{
	return IsSomeFilePath(fdi) &&
		IsSomeFileSize(fdi) &&
		IsSomeFileLastWriteTime(fdi) &&
		IsSomeFileAttribute(fdi);
}

BOOL CFileInformation::operator !=(CFileInformation fdi) const
{
	return !IsSomeFilePath(fdi) ||
		!IsSomeFileSize(fdi) ||
		!IsSomeFileLastWriteTime(fdi) ||
		!IsSomeFileAttribute(fdi);
}

const CFileInformation& CFileInformation::operator =(const CFileInformation& fdi)
{
	SetFileData(fdi.GetFileData());
	SetFileDir(fdi.GetFileDir());
	return *this;
}

BOOL CFileInformation::IsSomeFileData(WIN32_FIND_DATA fd) const
{
	WIN32_FIND_DATA fd2 = GetFileData();
	return (memcmp(&fd, &fd2, sizeof(WIN32_FIND_DATA)) == 0);
}

BOOL CFileInformation::IsSomeFileData(const CFileInformation& fdi) const
{
	WIN32_FIND_DATA fd1 = fdi.GetFileData();
	WIN32_FIND_DATA fd2 = GetFileData();
	return (memcmp(&fd1, &fd2, sizeof(WIN32_FIND_DATA)) == 0);
}

BOOL CFileInformation::IsSomeFileAttribute(DWORD dwAttribute) const
{
	DWORD dwAttribute2 = GetFileAttribute();
	return (dwAttribute2 == dwAttribute);
}

BOOL CFileInformation::IsSomeFileAttribute(const CFileInformation& fdi) const
{
	DWORD dwAttribute1 = fdi.GetFileAttribute();
	DWORD dwAttribute2 = GetFileAttribute();
	return (dwAttribute2 == dwAttribute1);
}

BOOL CFileInformation::IsSomeFileSize(DWORD dwFileSizeHigh, DWORD dwFileSizeLow) const
{
	DWORD dwFileSizeHigh2, dwFileSizeLow2;
	GetFileSize(dwFileSizeHigh2, dwFileSizeLow2);
	return ((dwFileSizeHigh2 == dwFileSizeHigh) &&
		(dwFileSizeLow2 == dwFileSizeLow));
}

BOOL CFileInformation::IsSomeFileSize(const CFileInformation& fdi) const
{
	DWORD dwFileSizeHigh1, dwFileSizeLow1;
	DWORD dwFileSizeHigh2, dwFileSizeLow2;
	fdi.GetFileSize(dwFileSizeHigh1, dwFileSizeLow1);
	GetFileSize(dwFileSizeHigh2, dwFileSizeLow2);
	return ((dwFileSizeHigh2 == dwFileSizeHigh1) &&
		(dwFileSizeLow2 == dwFileSizeLow1));
}

BOOL CFileInformation::IsSomeFileName(CString fileName) const
{
	CString fileName2 = GetFileName();
	fileName.MakeUpper();
	fileName2.MakeUpper();
	return (fileName2.Compare(fileName) == 0);
}

BOOL CFileInformation::IsSomeFileName(const CFileInformation& fdi) const
{
	CString fileName1 = GetFileName();
	CString fileName2 = fdi.GetFileName();
	fileName1.MakeUpper();
	fileName2.MakeUpper();
	return (fileName2.Compare(fileName1) == 0);
}

BOOL CFileInformation::IsSomeFileDir(CString fileDir) const
{
	CString fileDir2 = GetFileDir();
	fileDir.MakeUpper();
	fileDir2.MakeUpper();
	return (fileDir2.Compare(fileDir) == 0);
}

BOOL CFileInformation::IsSomeFileDir(const CFileInformation& fdi) const
{
	CString fileDir1 = GetFileDir();
	CString fileDir2 = fdi.GetFileDir();
	fileDir1.MakeUpper();
	fileDir2.MakeUpper();
	return (fileDir1.Compare(fileDir1) == 0);
}

BOOL CFileInformation::IsSomeFilePath(CString filePath) const
{
	CString filePath2 = GetFilePath();
	filePath.MakeUpper();
	filePath2.MakeUpper();
	return (filePath2.Compare(filePath) == 0);
}

BOOL CFileInformation::IsSomeFilePath(const CFileInformation& fdi) const
{
	CString filePath1 = GetFilePath();
	CString filePath2 = fdi.GetFilePath();
	filePath1.MakeUpper();
	filePath2.MakeUpper();
	return (filePath2.Compare(filePath1) == 0);
}

BOOL CFileInformation::IsSomeFileLastWriteTime(FILETIME fileTime) const
{
	FILETIME fileTime2 = GetFileLastWriteTime();

	return (fileTime.dwHighDateTime == fileTime2.dwHighDateTime &&
		fileTime.dwLowDateTime == fileTime2.dwLowDateTime);
}

BOOL CFileInformation::IsSomeFileLastWriteTime(const CFileInformation &fdi) const
{
	FILETIME fileTime1 = GetFileLastWriteTime();
	FILETIME fileTime2 = fdi.GetFileLastWriteTime();

	return (fileTime1.dwHighDateTime == fileTime2.dwHighDateTime &&
		fileTime1.dwLowDateTime == fileTime2.dwLowDateTime);
}

CString CFileInformation::ConcPath(const CString& first, const CString& second)
{
	CString path;

	if (first.IsEmpty() || first[first.GetLength() - 1] == L'\\')
		path = first + second;
	else
		path = first + _T("\\") + second;

	return path;
}

int CFileInformation::EnumFiles(CString root, P_FI_List list)
{
	WIN32_FIND_DATA ffd;
	CString         path = ConcPath(root, _T("*.*"));
	HANDLE          sh = FindFirstFile(path, &ffd);

	if (INVALID_HANDLE_VALUE == sh)
		return 0;

	do
	{
		CFileInformation* pFDI = new CFileInformation(ffd, root);

		if (pFDI->IsRootFile())
		{
			delete pFDI;
		}
		else if (pFDI->IsDirectory())
		{
			list->AddTail(pFDI);
			EnumFiles(ConcPath(root, pFDI->GetFileName()), list);
		}
		else if (pFDI->IsActualFile())
		{
			list->AddTail(pFDI);
		}
		else
		{
			delete pFDI;
		}
	} while (FindNextFile(sh, &ffd));

	FindClose(sh);

	return (int)list->GetCount();
}

int CFileInformation::EnumDirFiles(CString root, P_FI_List list)
{
	WIN32_FIND_DATA ffd;
	CString         path = ConcPath(root, _T("*.*"));
	HANDLE          sh = FindFirstFile(path, &ffd);

	if (INVALID_HANDLE_VALUE == sh)
		return 0;

	do
	{
		CFileInformation* pFDI = new CFileInformation(ffd, root);

		if (pFDI->IsRootFile())
		{
			delete pFDI;
		}
		else if (pFDI->IsDirectory())
		{
			list->AddTail(pFDI);
		}
		else if (pFDI->IsActualFile())
		{
			list->AddTail(pFDI);
		}
		else
		{
			delete pFDI;
		}
	} while (FindNextFile(sh, &ffd));

	FindClose(sh);

	return (int)list->GetCount();
}

int CFileInformation::EnumFilesExt(CString root, CString ext, P_FI_List list)
{
	WIN32_FIND_DATA ffd;
	CString         path = ConcPath(root, _T("*.*"));
	HANDLE          sh = FindFirstFile(path, &ffd);

	if (INVALID_HANDLE_VALUE == sh)
		return 0;

	do
	{
		CFileInformation* pFDI = new CFileInformation(ffd, root);

		if (pFDI->IsRootFile())
		{
			delete pFDI;
		}
		else if (pFDI->IsDirectory())
		{
			list->AddTail(pFDI);
			EnumFilesExt(ConcPath(root, pFDI->GetFileName()), ext, list);
		}
		else if (pFDI->IsActualFile() && pFDI->GetFileExt().CompareNoCase(ext) == 0)
		{
			list->AddTail(pFDI);
		}
		else
		{
			delete pFDI;
		}
	} while (FindNextFile(sh, &ffd));

	FindClose(sh);

	return (int)list->GetCount();
}

int CFileInformation::EnumDirFilesExt(CString root, CString ext, P_FI_List list)
{
	WIN32_FIND_DATA ffd;
	CString         path = ConcPath(root, _T("*.*"));
	HANDLE          sh = FindFirstFile(path, &ffd);

	if (INVALID_HANDLE_VALUE == sh)
		return 0;

	do
	{
		CFileInformation* pFDI = new CFileInformation(ffd, root);

		if (pFDI->IsRootFile())
		{
			delete pFDI;
		}
		else if (pFDI->IsDirectory())
		{
			list->AddTail(pFDI);
		}
		else if (pFDI->IsActualFile() && pFDI->GetFileExt().CompareNoCase(ext) == 0)
		{
			list->AddTail(pFDI);
		}
		else
		{
			delete pFDI;
		}
	} while (FindNextFile(sh, &ffd));

	FindClose(sh);

	return (int)list->GetCount();
}

void CFileInformation::SortFiles(P_FI_List list)
{
	if (list == NULL || list->GetCount() == 0)
		return;

	FI_List tempList;
	FI_List fileList;
	FI_List dirList;

	CopyFiles(list, &tempList);
	list->RemoveAll();

	POSITION          listPos = tempList.GetHeadPosition();
	CFileInformation* pFDI = NULL;

	while (listPos != NULL)
	{
		pFDI = tempList.GetNext(listPos);
		if (pFDI->IsDirectory())
			dirList.AddTail(pFDI);
		else
			fileList.AddTail(pFDI);
	}

	SortFilesABC(&dirList);
	SortFilesABC(&fileList);

	listPos = dirList.GetHeadPosition();
	while (listPos != NULL)
		list->AddTail(dirList.GetNext(listPos));

	listPos = fileList.GetHeadPosition();
	while (listPos != NULL)
		list->AddTail(fileList.GetNext(listPos));

	tempList.RemoveAll();
	fileList.RemoveAll();
	dirList.RemoveAll();

	return;
}

void CFileInformation::CopyFiles(const P_FI_List oldList, P_FI_List newList)
{
	if (oldList == NULL || newList == NULL || oldList->GetCount() == 0)
		return;

	POSITION listPos = oldList->GetHeadPosition();
	while (listPos != NULL)
		newList->AddTail(oldList->GetNext(listPos));

	return;
}

void CFileInformation::CopyFilesAndFI(const P_FI_List oldList, P_FI_List newList)
{
	if (oldList == NULL || newList == NULL || oldList->GetCount() == 0)
		return;

	POSITION listPos = oldList->GetHeadPosition();
	while (listPos != NULL)
		newList->AddTail(new CFileInformation(*oldList->GetNext(listPos)));

	return;
}

void CFileInformation::SortFilesABC(P_FI_List list)
{
	if (list == NULL || list->GetCount() == 0)
		return;

	CList< CFileInformation, CFileInformation> tempList;

	POSITION listPos = list->GetHeadPosition();
	while (listPos != NULL)
	{
		tempList.AddTail(list->GetAt(listPos));
		list->GetNext(listPos);
	}
	RemoveFiles(list);

	int              i,
		j,
		count = (int)tempList.GetCount();
	POSITION         pos1,
		pos2;
	CFileInformation fdi1,
		fdi2;

	for (j = 0; j < count; j++)
	{
		pos1 = tempList.GetHeadPosition();

		for (i = 0; i < count - 1; i++)
		{
			pos2 = pos1;
			fdi1 = tempList.GetNext(pos1);
			fdi2 = tempList.GetAt(pos1);
			if (fdi1.GetFileName().Compare(fdi2.GetFileName()) > 0)
			{
				tempList.SetAt(pos2, fdi2);
				tempList.SetAt(pos1, fdi1);
			}
		}
	}

	listPos = tempList.GetHeadPosition();
	while (listPos != NULL)
		list->AddTail(new CFileInformation(tempList.GetNext(listPos)));
	tempList.RemoveAll();
}

BOOL CFileInformation::RemoveFiles(P_FI_List list)
{
	if (list == NULL || list->GetCount() == 0)
		return FALSE;

	POSITION          listPos = list->GetHeadPosition();
	CFileInformation* pFDI = NULL;

	while (listPos != NULL)
	{
		pFDI = list->GetNext(listPos);

		if (pFDI == NULL)
			throw "pFDI == NULL";

		delete pFDI;
	}

	list->RemoveAll();

	return TRUE;
}

EFileAction CFileInformation::CompareFiles(P_FI_List oldList, P_FI_List newList, CFileInformation& fi)
{
	EFileAction       faType = faNone;
	POSITION          newListPos = NULL;
	POSITION          oldListPos = NULL;
	CFileInformation* newFI = NULL;
	CFileInformation* oldFI = NULL;
	int               nNew = (int)newList->GetCount();
	int               nOld = (int)oldList->GetCount();

	if (nOld == nNew)
	{
		newListPos = newList->GetHeadPosition();
		oldListPos = oldList->GetHeadPosition();
		while (oldListPos != NULL && newListPos != NULL)
		{
			oldFI = oldList->GetNext(oldListPos);
			newFI = newList->GetNext(newListPos);

			if (*oldFI != *newFI)
			{
				fi = *newFI;
				faType = faChange;
				break;
			}
		}
	}
	else if (nOld > nNew)
	{
		BOOL isFind;

		oldListPos = oldList->GetHeadPosition();
		while (oldListPos != NULL)
		{
			oldFI = oldList->GetNext(oldListPos);

			isFind = TRUE;

			newListPos = newList->GetHeadPosition();
			while (newListPos != NULL)
			{
				newFI = newList->GetNext(newListPos);

				if (*oldFI == *newFI)
				{
					isFind = FALSE;
					break;
				}
			}

			if (isFind)
			{
				fi = *oldFI;
				faType = faDelete;
				break;
			}
		}
	}
	else if (nOld < nNew)
	{
		BOOL isFind;

		newListPos = newList->GetHeadPosition();
		while (newListPos != NULL)
		{
			newFI = newList->GetNext(newListPos);

			isFind = TRUE;

			oldListPos = oldList->GetHeadPosition();
			while (oldListPos != NULL)
			{
				oldFI = oldList->GetNext(oldListPos);

				if (*oldFI == *newFI)
				{
					isFind = FALSE;
					break;
				}
			}

			if (isFind)
			{
				fi = *newFI;
				faType = faCreate;
				break;
			}
		}
	}

	return faType;
}

EFileAction CFileInformation::CompareFiles(P_FI_List oldList, P_FI_List newList, P_FI_List outList)
{
	EFileAction faType = faNone;

	POSITION newListPos = NULL;
	POSITION oldListPos = NULL;

	CFileInformation *newFI = NULL;
	CFileInformation *oldFI = NULL;

	int nNew = (int)newList->GetCount();
	int nOld = (int)oldList->GetCount();

	outList->RemoveAll();

	if (nOld == nNew)
	{
		newListPos = newList->GetHeadPosition();
		oldListPos = oldList->GetHeadPosition();
		while (oldListPos != NULL && newListPos != NULL)
		{
			oldFI = oldList->GetNext(oldListPos);
			newFI = newList->GetNext(newListPos);

			if (*oldFI != *newFI)
			{
				outList->AddTail(newFI);
				faType = faChange;
			}
		}
	}
	else if (nOld > nNew)
	{
		BOOL isFind;

		oldListPos = oldList->GetHeadPosition();
		while (oldListPos != NULL)
		{
			oldFI = oldList->GetNext(oldListPos);

			isFind = TRUE;

			newListPos = newList->GetHeadPosition();
			while (newListPos != NULL)
			{
				newFI = newList->GetNext(newListPos);

				if (*oldFI == *newFI)
				{
					isFind = FALSE;
					break;
				}
			}

			if (isFind)
			{
				outList->AddTail(oldFI);
				faType = faDelete;
			}
		}
	}
	else if (nOld < nNew)
	{
		BOOL isFind;

		newListPos = newList->GetHeadPosition();
		while (newListPos != NULL)
		{
			newFI = newList->GetNext(newListPos);

			isFind = TRUE;

			oldListPos = oldList->GetHeadPosition();
			while (oldListPos != NULL)
			{
				oldFI = oldList->GetNext(oldListPos);

				if (*oldFI == *newFI)
				{
					isFind = FALSE;
					break;
				}
			}

			if (isFind)
			{
				outList->AddTail(newFI);
				faType = faCreate;
			}
		}
	}

	return(faType);
}

BOOL CFileInformation::FindFilePath(CString root, CString& file)
{
	WIN32_FIND_DATA ffd;
	CString         path = ConcPath(root, _T("*.*"));
	HANDLE          sh = FindFirstFile(path, &ffd);
	BOOL            retval = FALSE;

	if (INVALID_HANDLE_VALUE == sh)
		return FALSE;

	do
	{
		CFileInformation fdi(ffd, root);

		if (fdi.IsRootFile())
			continue;

		if (fdi.IsDirectory() &&
			FindFilePath(ConcPath(root, fdi.GetFileName()), file))
		{
			retval = TRUE;
			break;
		}

		if (fdi.IsActualFile() && fdi.IsSomeFileName(file))
		{
			file = fdi.GetFilePath();
			retval = TRUE;
			break;
		}
	} while (FindNextFile(sh, &ffd));

	FindClose(sh);

	return retval;
}


BOOL CFileInformation::FindFilePathOnDisk(CString& file)
{
	DWORD dwDriveMask = GetLogicalDrives();
	int   i,
		max_drv = 26;

	TCHAR  szDir[4];	szDir[1] = TEXT(':'); szDir[2] = TEXT('\\'); szDir[3] = 0;

	// enumerate all logical, fixed drives
	for (i = 0; i < max_drv; dwDriveMask >>= 1, i++)
	{
		// if logical drive exists
		if (dwDriveMask & 0x01)
		{
			szDir[0] = TEXT('A') + i;

			if (GetDriveType(szDir) == DRIVE_FIXED && // if it is a fixed drive
				FindFilePath(szDir, file))
				return TRUE;
		}
	}

	return FALSE;
}

BOOL CFileInformation::FindFilePathOnCD(CString& file)
{
	DWORD dwDriveMask = GetLogicalDrives();
	int   i,
		max_drv = 26;

	TCHAR  szDir[4];	szDir[1] = TEXT(':'); szDir[2] = TEXT('\\'); szDir[3] = 0;

	// enumerate all logical, fixed drives
	for (i = 0; i < max_drv; dwDriveMask >>= 1, i++)
	{
		// if logical drive exists
		if (dwDriveMask & 0x01)
		{
			szDir[0] = TEXT('A') + i;

			if (GetDriveType(szDir) == DRIVE_CDROM && // if it is a cd drive
				FindFilePath(szDir, file))
				return TRUE;
		}
	}

	return FALSE;
}

BOOL CFileInformation::Load(CString file)
{
	if (!file.IsEmpty() && file[file.GetLength() - 1] == L'\\')
		file = file.Left(file.GetLength() - 1);

	WIN32_FIND_DATA ffd;
	HANDLE          hFind = FindFirstFile(file, &ffd);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		SetFileData(ffd);
		SetFileDir(GetFileDir(file));
		FindClose(hFind);
		return TRUE;
	}

	return FALSE;
}

BOOL CFileInformation::Load(CString dir, CString file)
{
	WIN32_FIND_DATA ffd;
	HANDLE          hFind = FindFirstFile(ConcPath(dir, file), &ffd);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		SetFileData(ffd);
		SetFileDir(dir);
		FindClose(hFind);
		return TRUE;
	}

	return FALSE;
}

CString CFileInformation::GetFileDir(CString path) const
{
	CString dir;
	dir.Empty();
	int     pos = path.ReverseFind(L'\\');

	if (pos != -1)
		dir = path.Left(pos);

	return dir;
}

BOOL CFileInformation::IsFileExist() const
{
	CString path = GetFilePath();

	if (path.IsEmpty())
		return FALSE;

	CFile cf;
	if (!cf.Open(path, CFile::modeRead | CFile::shareDenyNone))
		return FALSE;
	cf.Close();

	return TRUE;
}

void CFileInformation::CopyDir(CString oldRoot, CString newRoot)
{
	if (oldRoot.IsEmpty() || newRoot.IsEmpty())
		return;

	CFileInformation* pFDI;
	WIN32_FIND_DATA   ffd;
	HANDLE            sh = FindFirstFile(CFileInformation::ConcPath(oldRoot, _T("*.*")), &ffd);

	if (INVALID_HANDLE_VALUE == sh)
		return;

	CFileInformation::CreateDir(newRoot);

	do
	{
		pFDI = new CFileInformation(ffd, oldRoot);

		if (pFDI->IsRootFile())
		{
			delete pFDI;
			continue;
		}

		if (pFDI->IsDirectory())
		{
			CopyDir(CFileInformation::ConcPath(oldRoot, pFDI->GetFileName()),
				CFileInformation::ConcPath(newRoot, pFDI->GetFileName()));
		}

		if (pFDI->IsActualFile())
		{
			CopyFile(pFDI->GetFilePath(),
				CFileInformation::ConcPath(newRoot, pFDI->GetFileName()),
				FALSE);
		}

		delete pFDI;
	} while (FindNextFile(sh, &ffd));

	FindClose(sh);
}

void CFileInformation::MoveDir(CString oldRoot, CString newRoot)
{
	if (oldRoot.IsEmpty() || newRoot.IsEmpty())
		return;

	CFileInformation* pFDI;
	WIN32_FIND_DATA   ffd;
	HANDLE            sh = FindFirstFile(CFileInformation::ConcPath(oldRoot, _T("*.*")), &ffd);

	if (INVALID_HANDLE_VALUE == sh)
		return;

	CFileInformation::CreateDir(newRoot);

	do
	{
		pFDI = new CFileInformation(ffd, oldRoot);

		if (pFDI->IsRootFile())
		{
			delete pFDI;
			continue;
		}

		if (pFDI->IsDirectory())
		{
			MoveDir(CFileInformation::ConcPath(oldRoot, pFDI->GetFileName()),
				CFileInformation::ConcPath(newRoot, pFDI->GetFileName()));
		}

		if (pFDI->IsActualFile())
		{
			try
			{
				CFile::Rename(pFDI->GetFilePath(),
					CFileInformation::ConcPath(newRoot, pFDI->GetFileName()));
			}
			catch (...)
			{
			}
		}

		delete pFDI;
	} while (FindNextFile(sh, &ffd));

	RemoveDir(oldRoot);

	FindClose(sh);
}

void CFileInformation::RemoveDir(CString dir)
{
	RemoveDirectory(dir);

	WIN32_FIND_DATA find;
	HANDLE          hndle;
	TCHAR           *strFindFiles = new TCHAR[MAX_PATH];

	memset(strFindFiles, 0, MAX_PATH);
	_tcscpy(strFindFiles, dir.GetBuffer(dir.GetLength()));
	_tcscat(strFindFiles, _T("\\*.*"));

	hndle = FindFirstFile(strFindFiles, &find);

	while (hndle != INVALID_HANDLE_VALUE)
	{
		TCHAR *strFolderItem = new TCHAR[MAX_PATH];

		memset(strFolderItem, 0, MAX_PATH);
		_tcscpy(strFolderItem, dir.GetBuffer(dir.GetLength()));
		_tcscat(strFolderItem, _T("\\"));
		_tcscat(strFolderItem, find.cFileName);

		if ((!_tcscmp(find.cFileName, _T("."))) ||
			(!_tcscmp(find.cFileName, _T(".."))))
		{
			RemoveDirectory(strFolderItem);

			delete strFolderItem;
			strFolderItem = nullptr;

			if (FindNextFile(hndle, &find))
			{
				continue;
			}
			else
			{
				RemoveDirectory(dir);
				break;
			}
		}

		if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			RemoveDir(strFolderItem);
		}
		else
		{
			SetFileAttributes(strFolderItem, FILE_ATTRIBUTE_NORMAL);
			DeleteFile(strFolderItem);
		}

		delete strFolderItem;
		strFolderItem = nullptr;

		if (FindNextFile(hndle, &find))
		{
			continue;
		}
		else
		{
			break;
		}
	}

	delete strFindFiles;
	strFindFiles = nullptr;

	FindClose(hndle);
	SetFileAttributes(dir, FILE_ATTRIBUTE_DIRECTORY);
	RemoveDirectory(dir);
}

void CFileInformation::CreateDir(CString dir)
{
	if (dir.IsEmpty())
		return;

	if (dir[dir.GetLength() - 1] == L'\\')
		dir = dir.Left(dir.GetLength() - 1);

	if (dir[dir.GetLength() - 1] == L':')
		return;

	int pos = dir.ReverseFind(L'\\');
	CreateDir(dir.Left(pos));

	CreateDirectory(dir, NULL);
}

BOOL CFileInformation::IsOk() const
{
	CString dir = GetFileDir();
	CString name = GetFileName();

	if (IsDirectory())
		return !dir.IsEmpty();
	else
		return !dir.IsEmpty() && !name.IsEmpty();
}

void CFileInformation::ParseDir(CString root, DirParsCallback action, LPVOID pData)
{
	if (root.IsEmpty() || action == NULL)
		return;

	CFileInformation* pFDI;
	WIN32_FIND_DATA   ffd;
	HANDLE            sh = FindFirstFile(CFileInformation::ConcPath(root, _T("*.*")), &ffd);

	if (INVALID_HANDLE_VALUE == sh)
		return;

	action(root, pData);

	do
	{
		pFDI = new CFileInformation(ffd, root);

		if (pFDI->IsRootFile())
		{
			delete pFDI;
			continue;
		}

		if (pFDI->IsDirectory())
		{
			ParseDir(CFileInformation::ConcPath(root, pFDI->GetFileName()), action, pData);
		}

		if (pFDI->IsActualFile())
		{
			action(pFDI->GetFilePath(), pData);
		}

		delete pFDI;
	} while (FindNextFile(sh, &ffd));

	FindClose(sh);
}

UINT CFileInformation::GetPathLevel(CString path)
{
	UINT n = 0;
	int  pos = 0;

	do
	{
		pos = path.Find(L'\\', pos);
		if (pos++ > 0)
			n++;
	} while (pos > 0);

	return n;
}

CString CFileInformation::GenerateNewFileName(CString path)
{
	CString newPath = path;
	FILE*   pFile = _tfopen(newPath, _T("r"));

	while (pFile != NULL)
	{
		fclose(pFile);

		CFileInformation fi(newPath);
		newPath = fi.GetFileDir() + _T("\\_") + fi.GetFileName();

		pFile = _tfopen(newPath, _T("r"));
	}

	return newPath;
}

CString CFileInformation::GetFileDirectory(CString path)
{
	CString dir(_T(""));
	int     pos = path.ReverseFind(L'\\');

	if (pos != -1)
		dir = path.Left(pos);

	return dir;
}

CString CFileInformation::GetFileName(CString path)
{
	CString name(_T(""));
	int     pos = path.ReverseFind(L'\\');

	if (pos != -1)
		name = path.Right(path.GetLength() - pos - 1);

	return name;
}

CString CFileInformation::GetFileNameWithoutExt(CString path)
{
	CString name = GetFileName(path);
	int     pos = name.ReverseFind(L'.');

	if (pos != -1)
		name = name.Left(pos);

	return name;
}

CString CFileInformation::GetFileExt(CString path)
{
	CString ext = GetFileName(path);
	int     pos = ext.ReverseFind(L'.');

	if (pos != -1)
		ext = ext.Right(ext.GetLength() - pos - 1);

	return ext;
}
