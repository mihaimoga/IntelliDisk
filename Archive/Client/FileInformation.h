// FileInformation.h: interface for the CFileInformation class.
//
//////////////////////////////////////////////////////////////////////
//
// Youry M. Jukov, Israel
//
// Last update - 29.07.2003 
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FILEINFORMATION_H__63095D3F_570E_4511_BBE5_AA37861E33E1__INCLUDED_)
#define AFX_FILEINFORMATION_H__63095D3F_570E_4511_BBE5_AA37861E33E1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
#error include 'stdafx.h' before including this file for PCH
#endif

#include <afxtempl.h>	//for template collections

class   CFileInformation;
typedef CTypedPtrList<CObList, CFileInformation*> FI_List;
typedef FI_List* P_FI_List;

typedef enum FileAction { faNone, faDelete, faCreate, faChange, } EFileAction;
typedef enum FileSize { fsBytes, fsKBytes, fsMBytes, } EFileSize;

#define IS_NOTACT_FILE( action ) ( action == faNone   )
#define IS_CREATE_FILE( action ) ( action == faCreate )
#define IS_DELETE_FILE( action ) ( action == faDelete )
#define IS_CHANGE_FILE( action ) ( action == faChange )

typedef UINT(*DirParsCallback)(CString path, LPVOID pData);

class CFileInformation : public CObject
{
public:
	CFileInformation();
	CFileInformation(CString path);
	CFileInformation(CString dir, CString file);
	CFileInformation(WIN32_FIND_DATA fd, CString dir);
	CFileInformation(const CFileInformation& fdi);
	CFileInformation(const CFileInformation* fdi);
	virtual ~CFileInformation();

	const CFileInformation& operator =(const CFileInformation& fdi);
	BOOL operator ==(CFileInformation fdi) const;
	BOOL operator !=(CFileInformation fdi) const;

	BOOL Load(CString file);
	BOOL Load(CString dir, CString file);
	CString GetFileDir(CString path) const;

	BOOL IsFileExist() const;
	BOOL IsNotAvailableNow() const;
	BOOL IsSystem() const;
	BOOL IsReadOnly() const;
	BOOL IsHidden() const;
	BOOL IsNormal() const;
	BOOL IsArchive() const;
	BOOL IsDirectory() const;
	BOOL IsCurrentRoot() const;
	BOOL IsParentDir() const;
	BOOL IsRootFile() const;
	BOOL IsTemporary() const;
	BOOL IsWinTemporary() const;
	BOOL IsActualFile() const;
	BOOL IsOk() const;
	BOOL IsSomeFileName(CString fileName) const;
	BOOL IsSomeFileName(const CFileInformation& fdi) const;
	BOOL IsSomeFileDir(CString fileDir) const;
	BOOL IsSomeFileDir(const CFileInformation& fdi) const;
	BOOL IsSomeFilePath(CString filePath) const;
	BOOL IsSomeFilePath(const CFileInformation& fdi) const;
	BOOL IsSomeFileData(WIN32_FIND_DATA fd) const;
	BOOL IsSomeFileData(const CFileInformation& fdi) const;
	BOOL IsSomeFileSize(DWORD dwFileSizeHigh, DWORD dwFileSizeLow) const;
	BOOL IsSomeFileSize(const CFileInformation& fdi) const;
	BOOL IsSomeFileAttribute(DWORD dwAttribute) const;
	BOOL IsSomeFileAttribute(const CFileInformation& fdi) const;
	BOOL IsSomeFileLastWriteTime(FILETIME fileTime) const;
	BOOL IsSomeFileLastWriteTime(const CFileInformation& fdi) const;

	BOOL FileAttributeReadOnly(BOOL set = TRUE);

	FILETIME        GetFileLastWriteTime() const;
	CString         GetFilePath() const;
	void            SetFileDir(const CString& dir);
	CString         GetFileDir() const;
	DWORD           GetFileAttribute() const;
	unsigned long   GetFileSize(DWORD& dwFileSizeHigh, DWORD& dwFileSizeLow) const;
	unsigned long   GetFileSize(EFileSize& fsFormat) const;
	CString         GetFileName() const;
	CString         GetFileNameWithoutExt() const;
	CString         GetFileExt() const;
	void            ZeroFileData();
	WIN32_FIND_DATA GetFileData() const;
	void            SetFileData(WIN32_FIND_DATA fd);

	static CString		ConcPath(const CString& first, const CString& second);
	static int 			EnumDirFiles(CString root, P_FI_List list);
	static int 			EnumFiles(CString root, P_FI_List list);
	static int 			EnumDirFilesExt(CString root, CString ext, P_FI_List list);
	static int 			EnumFilesExt(CString root, CString ext, P_FI_List list);
	static void			CopyFiles(const P_FI_List oldList, P_FI_List newList);
	static void			CopyFilesAndFI(const P_FI_List oldList, P_FI_List newList);
	static void			SortFiles(P_FI_List list);
	static void			SortFilesABC(P_FI_List list);
	static BOOL			RemoveFiles(P_FI_List list);
	static BOOL			FindFilePath(CString root, CString& file);
	static EFileAction	CompareFiles(P_FI_List oldList, P_FI_List newList, CFileInformation& fi);
	static EFileAction	CompareFiles(P_FI_List oldList, P_FI_List newList, P_FI_List outList);
	static BOOL			FindFilePathOnDisk(CString& file);
	static BOOL			FindFilePathOnCD(CString& file);
	static void			CopyDir(CString oldRoot, CString newRoot);
	static void			MoveDir(CString oldRoot, CString newRoot);
	static void         RemoveDir(CString dir);
	static void			CreateDir(CString dir);
	static void         ParseDir(CString root, DirParsCallback action, LPVOID pData);
	static UINT         GetPathLevel(CString path);
	static CString      GenerateNewFileName(CString path);
	static CString      GetFileDirectory(CString path);
	static CString      GetFileName(CString path);
	static CString      GetFileNameWithoutExt(CString path);
	static CString      GetFileExt(CString path);

protected:
	CString m_dir;
	WIN32_FIND_DATA m_fd;
};

#endif // !defined(AFX_FILEINFORMATION_H__63095D3F_570E_4511_BBE5_AA37861E33E1__INCLUDED_)