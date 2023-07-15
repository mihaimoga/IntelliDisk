/* This file is part of IntelliDisk application developed by Stefan-Mihai MOGA.

IntelliDisk is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Open
Source Initiative, either version 3 of the License, or any later version.

IntelliDisk is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
IntelliDisk. If not, see <http://www.opensource.org/licenses/gpl-3.0.html>*/

#include "pch.h"
#include "IntelliDiskExt.h"
#include "MainFrame.h"
#include <KnownFolders.h>
#include <shlobj.h>
#include "SHA256.h"

#define SECURITY_WIN32
#include "Security.h"
#pragma comment(lib, "Secur32.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

std::wstring utf8_to_wstring(const std::string& str)
{
	// convert UTF-8 string to wstring
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	return myconv.from_bytes(str);
}

std::string wstring_to_utf8(const std::wstring& str)
{
	// convert wstring to UTF-8 string
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	return myconv.to_bytes(str);
}

std::wstring encode_filepath(std::wstring strResult)
{
	const std::wstring strApplicationName = _T("IntelliDisk\\");
	const std::wstring strSpecialFolder = GetSpecialFolder();
	if (strResult.find(strSpecialFolder) == 0)
		return strResult.replace(0, strSpecialFolder.length(), strApplicationName);
	return strResult;
}

std::wstring decode_filepath(std::wstring strResult)
{
	const std::wstring strApplicationName = _T("IntelliDisk\\");
	const std::wstring strSpecialFolder = GetSpecialFolder();
	if (strResult.find(strApplicationName) == 0)
		return strResult.replace(0, strApplicationName.length(), strSpecialFolder);
	return strResult;
}

const std::string GetMachineID()
{
	/* LINUX:
	#include <unistd.h>
	#include <limits.h>

	char hostname[HOST_NAME_MAX];
	char username[LOGIN_NAME_MAX];
	gethostname(hostname, HOST_NAME_MAX);
	getlogin_r(username, LOGIN_NAME_MAX);
	*/

	DWORD nLength = 0x1000;
	TCHAR lpszUserName[0x1000] = { 0, };
	if (GetUserNameEx(NameUserPrincipal, lpszUserName, &nLength))
	{
		lpszUserName[nLength] = 0;
		TRACE(_T("UserName = %s\n"), lpszUserName);
	}
	else
	{
		nLength = 0x1000;
		if (GetUserName(lpszUserName, &nLength) != 0)
		{
			lpszUserName[nLength] = 0;
			TRACE(_T("UserName = %s\n"), lpszUserName);
		}
	}

	nLength = 0x1000;
	TCHAR lpszComputerName[0x1000] = { 0, };
	if (GetComputerNameEx(ComputerNamePhysicalDnsFullyQualified, lpszComputerName, &nLength))
	{
		lpszComputerName[nLength] = 0;
		TRACE(_T("ComputerName = %s\n"), lpszComputerName);
	}
	else
	{
		nLength = 0x1000;
		if (GetComputerName(lpszComputerName, &nLength) != 0)
		{
			lpszComputerName[nLength] = 0;
			TRACE(_T("ComputerName = %s\n"), lpszComputerName);
		}
	}

	std::wstring result(lpszUserName);
	result += _T(":");
	result += lpszComputerName;
	return wstring_to_utf8(result);
}

const std::wstring GetSpecialFolder()
{
	WCHAR* lpszSpecialFolderPath = nullptr;
	if ((SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &lpszSpecialFolderPath)) == S_OK)
	{
		std::wstring result(lpszSpecialFolderPath);
		CoTaskMemFree(lpszSpecialFolderPath);
		result += _T("\\IntelliDisk\\");
		return result;
	}
	return _T("");
}

bool InstallStartupApps(bool bInstallStartupApps)
{
	HKEY regValue;
	bool result = false;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_ALL_ACCESS, &regValue) == ERROR_SUCCESS)
	{
		if (bInstallStartupApps) // Add program to Starup Apps
		{
			TCHAR lpszApplicationBuffer[MAX_PATH + 1] = { 0, };
			if (GetModuleFileName(nullptr, lpszApplicationBuffer, MAX_PATH) > 0)
			{
				std::wstring quoted(_T("\""));
				quoted += lpszApplicationBuffer;
				quoted += _T("\"");
				const size_t length = quoted.length() * sizeof(TCHAR);
				if (RegSetValueEx(regValue, _T("IntelliDisk"), 0, REG_SZ, (LPBYTE)quoted.c_str(), (DWORD)length) == ERROR_SUCCESS)
				{
					result = true;
				}
			}
		}
		else // Remove program from Startup Apps
		{
			if (RegDeleteValue(regValue, _T("IntelliDisk")) == ERROR_SUCCESS)
			{
				result = true;
			}
		}
		RegCloseKey(regValue);
	}
	return result;
}

UINT DirCallback(CFileInformation fiObject, EFileAction faAction, LPVOID lpData)
{
	const int nFileEvent = (IS_DELETE_FILE(faAction)) ? ID_FILE_DELETE : ID_FILE_UPLOAD;
	const std::wstring strFilePath = fiObject.GetFilePath();
	AddNewItem(nFileEvent, strFilePath, lpData);

	return 0; // success
}

const int MAX_BUFFER = 0x10000;
int g_nPingCount = 0;
bool g_bThreadRunning = true;
bool g_bIsConnected = false;

const char HEX_MAP[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
char replace(unsigned char c)
{
	return HEX_MAP[c & 0x0f];
}

std::string char_to_hex(unsigned char c)
{
	std::string hex;
	// First four bytes
	char left = (c >> 4);
	// Second four bytes
	char right = (c & 0x0f);

	hex += replace(left);
	hex += replace(right);
	return hex;
}

std::wstring dumpHEX(unsigned char* pBuffer, const int nLength)
{
	std::string result;
	for (int nIndex = 0; nIndex < nLength; nIndex++)
	{
		result += char_to_hex(pBuffer[nIndex]);
		result += " ";
	}
	return utf8_to_wstring(result);
}

bool ReadBuffer(CWSocket& pApplicationSocket, unsigned char* pBuffer, int& nLength, const bool ReceiveENQ, const bool ReceiveEOT)
{
	int nIndex = 0;
	int nCount = 0;
	char nReturn = ACK;

	try
	{
		if (ReceiveENQ)
		{
			nLength = MAX_BUFFER;
			if (pApplicationSocket.IsReadible(1000) &&
				((nLength = pApplicationSocket.Receive(pBuffer, nLength)) > 0) &&
				(ENQ == pBuffer[nLength - 1]))
			{
				TRACE(_T("ENQ Received\n"));
				unsigned char chACK = ACK;
				VERIFY(pApplicationSocket.Send(&chACK, sizeof(chACK)) == 1);
				TRACE(_T("ACK Sent\n"));
			}
			else
				return false;
		}
		nLength = 0;
		do {
			while (pApplicationSocket.IsReadible(1000) &&
				((nIndex = pApplicationSocket.Receive(pBuffer + nIndex, MAX_BUFFER - nLength)) > 0))
			{
				nLength += nIndex;
				TRACE(_T("Buffer Received %s\n"), dumpHEX(pBuffer, nLength).c_str());
				nReturn = (pBuffer[nLength - 1] == calcLRC(&pBuffer[3], (nLength - 5))) ? ACK : NAK;
			}
			VERIFY(pApplicationSocket.Send(&nReturn, sizeof(nReturn)) == 1);
			TRACE(_T("%s Sent\n"), ((ACK == nReturn) ? _T("ACK") : _T("NAK")));
		} while ((ACK != nReturn) && (++nCount < 3));
		if (ReceiveEOT)
		{
			unsigned char chEOT = ACK;
			if (pApplicationSocket.IsReadible(1000) &&
				((nLength = pApplicationSocket.Receive(&chEOT, sizeof(chEOT))) > 0) &&
				(EOT == chEOT))
			{
				TRACE(_T("EOT Received\n"));
			}
			else
				return false;
		}
	}
	catch (CWSocketException* pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		pException->Delete();
		pApplicationSocket.Close();
		g_bIsConnected = false;
		return false;
	}
	TRACE(_T("ReadBuffer: %s\n"), ((ACK == nReturn) ? _T("true") : _T("false")));
	return (ACK == nReturn);
}

bool WriteBuffer(CWSocket& pApplicationSocket, const unsigned char* pBuffer, const int nLength, const bool SendENQ, const bool SendEOT)
{
	int nCount = 0;
	unsigned char nReturn = ACK;
	unsigned char pPacket[MAX_BUFFER] = { 0, };

	try
	{
		ASSERT(nLength <= (sizeof(pPacket) - 5));
		if (SendENQ && pApplicationSocket.IsWritable(1000))
		{
			unsigned char chENQ = ENQ;
			TRACE(_T("ENQ Sent\n"));
			VERIFY(pApplicationSocket.Send(&chENQ, sizeof(chENQ)) == 1);
			ZeroMemory(pPacket, sizeof(pPacket));
			nCount = MAX_BUFFER;
			if (((nCount = pApplicationSocket.Receive(pPacket, nCount)) > 0) &&
				(ACK == pPacket[nCount - 1]))
			{
				TRACE(_T("ACK Received\n"));
				nCount = 0;
			}
			else
				return false;
		}
		ZeroMemory(pPacket, sizeof(pPacket));
		CopyMemory(&pPacket[3], pBuffer, nLength);
		pPacket[0] = STX;
		pPacket[1] = (char)(nLength / 0x100);
		pPacket[2] = nLength % 0x100;
		pPacket[3 + nLength] = ETX;
		pPacket[4 + nLength] = calcLRC(pBuffer, nLength);
		do {
			if (pApplicationSocket.Send(pPacket, (5 + nLength)) == (5 + nLength))
			{
				TRACE(_T("Buffer Sent %s\n"), dumpHEX(pPacket, (5 + nLength)).c_str());
				VERIFY(pApplicationSocket.Receive(&nReturn, sizeof(nReturn)) == 1);
				TRACE(_T("%s Received\n"), ((ACK == nReturn) ? _T("ACK") : _T("NAK")));
			}
		} while ((nReturn != ACK) && (++nCount <= 3));
		if (SendEOT && pApplicationSocket.IsWritable(1000))
		{
			unsigned char chEOT = EOT;
			TRACE(_T("EOT Sent\n"));
			VERIFY(pApplicationSocket.Send(&chEOT, sizeof(chEOT)) == 1);
		}
	}
	catch (CWSocketException* pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		pException->Delete();
		pApplicationSocket.Close();
		g_bIsConnected = false;
		return false;
	}
	TRACE(_T("WriteBuffer: %s\n"), ((ACK == nReturn) ? _T("true") : _T("false")));
	return (ACK == nReturn);
}

std::wstring g_strCurrentDocument;
bool DownloadFile(CWSocket& pApplicationSocket, const std::wstring& strFilePath)
{
	SHA256 pSHA256;
	unsigned char pFileBuffer[MAX_BUFFER] = { 0, };
	try
	{
		g_strCurrentDocument = strFilePath;
		TRACE(_T("[DownloadFile] %s\n"), strFilePath.c_str());
		CFile pBinaryFile(strFilePath.c_str(), CFile::modeWrite | CFile::modeCreate | CFile::typeBinary);
		ULONGLONG nFileLength = 0;
		int nLength = (int)(sizeof(nFileLength) + 5);
		ZeroMemory(pFileBuffer, sizeof(pFileBuffer));
		if (ReadBuffer(pApplicationSocket, pFileBuffer, nLength, false, false))
		{
			CopyMemory(&nFileLength, &pFileBuffer[3], sizeof(nFileLength));
			TRACE(_T("nFileLength = %llu\n"), nFileLength);

			ULONGLONG nFileIndex = 0;
			while (nFileIndex < nFileLength)
			{
				nLength = (int)sizeof(pFileBuffer);
				ZeroMemory(pFileBuffer, sizeof(pFileBuffer));
				if (ReadBuffer(pApplicationSocket, pFileBuffer, nLength, false, false))
				{
					nFileIndex += (nLength - 5);
					pSHA256.update(&pFileBuffer[3], nLength - 5);

					pBinaryFile.Write(&pFileBuffer[3], nLength - 5);
				}
			}
		}
		else
		{
			TRACE(_T("Invalid nFileLength!\n"));
			pBinaryFile.Close();
			return false;
		}
		const std::string strDigestSHA256 = pSHA256.toString(pSHA256.digest());
		nLength = (int)strDigestSHA256.length() + 5;
		ZeroMemory(pFileBuffer, sizeof(pFileBuffer));
		if (ReadBuffer(pApplicationSocket, pFileBuffer, nLength, false, true))
		{
			const std::string strCommand = (char*)&pFileBuffer[3];
			if (strDigestSHA256.compare(strCommand) != 0)
			{
				TRACE(_T("Invalid SHA256!\n"));
				pBinaryFile.Close();
				return false;
			}
		}
		pBinaryFile.Close();
		g_strCurrentDocument.empty();
	}
	catch (CFileException* pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		pException->Delete();
		return false;
	}
	return true;
}

bool UploadFile(CWSocket& pApplicationSocket, const std::wstring& strFilePath)
{
	SHA256 pSHA256;
	unsigned char pFileBuffer[MAX_BUFFER] = { 0, };
	try
	{
		TRACE(_T("[UploadFile] %s\n"), strFilePath.c_str());
		CFile pBinaryFile(strFilePath.c_str(), CFile::modeRead | CFile::typeBinary);
		ULONGLONG nFileLength = pBinaryFile.GetLength();
		int nLength = sizeof(nFileLength);
		if (WriteBuffer(pApplicationSocket, (unsigned char*)&nFileLength, nLength, false, false))
		{
			ULONGLONG nFileIndex = 0;
			while (nFileIndex < nFileLength)
			{
				nLength = pBinaryFile.Read(pFileBuffer, MAX_BUFFER - 5);
				nFileIndex += nLength;
				if (WriteBuffer(pApplicationSocket, pFileBuffer, nLength, false, false))
				{
					pSHA256.update(pFileBuffer, nLength);
				}
				else
				{
					pBinaryFile.Close();
					return false;
				}
			}
		}
		else
		{
			TRACE(_T("Invalid nFileLength!\n"));
			pBinaryFile.Close();
			return false;
		}
		const std::string strDigestSHA256 = pSHA256.toString(pSHA256.digest());
		nLength = (int)strDigestSHA256.length() + 1;
		if (WriteBuffer(pApplicationSocket, (unsigned char*)strDigestSHA256.c_str(), nLength, false, true))
		{
			TRACE(_T("Upload Done!\n"));
		}
		else
		{
			pBinaryFile.Close();
			return false;
		}
		pBinaryFile.Close();
	}
	catch (CFileException* pException)
	{
		const int nErrorLength = 0x100;
		TCHAR lpszErrorMessage[nErrorLength] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
		TRACE(_T("%s\n"), lpszErrorMessage);
		pException->Delete();
		return false;
	}
	return true;
}

DWORD WINAPI ProducerThread(LPVOID lpParam)
{
	unsigned char pBuffer[MAX_BUFFER] = { 0, };
	int nLength = 0;

	CMainFrame* pMainFrame = (CMainFrame*)lpParam;
	CWSocket& pApplicationSocket = pMainFrame->m_pApplicationSocket;
	HANDLE& hSocketMutex = pMainFrame->m_hSocketMutex;

	while (g_bThreadRunning)
	{
		try
		{
			WaitForSingleObject(hSocketMutex, INFINITE);

			if (!pApplicationSocket.IsCreated())
			{
				pApplicationSocket.CreateAndConnect(pMainFrame->m_strServerIP, pMainFrame->m_nServerPort);
				const std::string strCommand = "IntelliDisk";
				nLength = (int)strCommand.length() + 1;
				if (WriteBuffer(pApplicationSocket, (unsigned char*) strCommand.c_str(), nLength, true, false))
				{
					TRACE(_T("Client connected!\n"));
					const std::string strMachineID = GetMachineID();
					int nComputerLength = (int)strMachineID.length() + 1;
					if (WriteBuffer(pApplicationSocket, (unsigned char*) strMachineID.c_str(), nComputerLength, false, true))
					{
						TRACE(_T("Logged In!\n"));
						g_bIsConnected = true;
						MessageBeep(MB_OK);
					}
				}
			}
			else
			{
				if (pApplicationSocket.IsReadible(1000))
				{
					g_nPingCount = 0;
					nLength = sizeof(pBuffer);
					ZeroMemory(pBuffer, sizeof(pBuffer));
					if (ReadBuffer(pApplicationSocket, pBuffer, nLength, true, false))
					{
						const std::string strCommand = (char*)&pBuffer[3];
						TRACE(_T("strCommand = %s\n"), utf8_to_wstring(strCommand).c_str());
						if (strCommand.compare("Restart") == 0)
						{
							nLength = sizeof(pBuffer);
							ZeroMemory(pBuffer, sizeof(pBuffer));
							if (((nLength = pApplicationSocket.Receive(pBuffer, nLength)) > 0) &&
								(EOT == pBuffer[nLength - 1]))
							{
								TRACE(_T("EOT Received\n"));
							}
							TRACE(_T("Restart!\n"));
							pApplicationSocket.Close();
							g_bIsConnected = false;
						}
						else if (strCommand.compare("NotifyDownload") == 0)
						{
							nLength = sizeof(pBuffer);
							ZeroMemory(pBuffer, sizeof(pBuffer));
							if (ReadBuffer(pApplicationSocket, pBuffer, nLength, false, false))
							{
								CString strMessage;
								const std::wstring strUNICODE = decode_filepath(utf8_to_wstring((char*)&pBuffer[3]));
								strMessage.Format(_T("Downloading %s..."), strUNICODE.c_str());
								pMainFrame->ShowMessage(strMessage.GetBuffer(), strUNICODE);
								strMessage.ReleaseBuffer();

								TRACE(_T("Downloading %s...\n"), strUNICODE.c_str());
								VERIFY(DownloadFile(pApplicationSocket, strUNICODE));
							}
						}
						else if (strCommand.compare("NotifyDelete") == 0)
						{
							nLength = sizeof(pBuffer);
							ZeroMemory(pBuffer, sizeof(pBuffer));
							if (ReadBuffer(pApplicationSocket, pBuffer, nLength, false, false))
							{
								const std::wstring strUNICODE = decode_filepath(utf8_to_wstring((char*)&pBuffer[3]));
								VERIFY(DeleteFile(strUNICODE.c_str()));
							}
						}
					}
				}
				else
				{
					if (60 == ++g_nPingCount) // every 60 seconds
					{
						g_nPingCount = 0;
						const std::string strCommand = "Ping";
						nLength = (int)strCommand.length() + 1;
						if (WriteBuffer(pApplicationSocket, (unsigned char*) strCommand.c_str(), nLength, true, true))
						{
							TRACE(_T("Ping!\n"));
						}
					}
				}
			}

			ReleaseSemaphore(hSocketMutex, 1, nullptr);
		}
		catch (CWSocketException* pException)
		{
			const int nErrorLength = 0x100;
			TCHAR lpszErrorMessage[nErrorLength] = { 0, };
			pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
			TRACE(_T("%s\n"), lpszErrorMessage);
			pException->Delete();
			ReleaseSemaphore(hSocketMutex, 1, nullptr);
			pApplicationSocket.Close();
			g_bIsConnected = false;
			Sleep(1000);
			continue;
		}
	}
	TRACE(_T("exiting...\n"));
	return 0;
}

DWORD WINAPI ConsumerThread(LPVOID lpParam)
{
	int nLength = 0;

	CMainFrame* pMainFrame = (CMainFrame*)lpParam;
	HANDLE& hOccupiedSemaphore = pMainFrame->m_hOccupiedSemaphore;
	HANDLE& hEmptySemaphore = pMainFrame->m_hEmptySemaphore;
	HANDLE& hResourceMutex = pMainFrame->m_hResourceMutex;
	HANDLE& hSocketMutex = pMainFrame->m_hSocketMutex;

	CWSocket& pApplicationSocket = pMainFrame->m_pApplicationSocket;

	while (g_bThreadRunning)
	{
		WaitForSingleObject(hOccupiedSemaphore, INFINITE);
		WaitForSingleObject(hResourceMutex, INFINITE);

		const int nFileEvent = pMainFrame->m_pResourceArray[pMainFrame->m_nNextOut].nFileEvent;
		const std::wstring& strFilePath = pMainFrame->m_pResourceArray[pMainFrame->m_nNextOut].strFilePath;
		TRACE(_T("[ConsumerThread] nFileEvent = %d, strFilePath = \"%s\"\n"), nFileEvent, strFilePath.c_str());
		pMainFrame->m_nNextOut++;
		pMainFrame->m_nNextOut %= NOTIFY_FILE_SIZE;

		if (ID_STOP_PROCESS == nFileEvent)
		{
			TRACE(_T("Stopping...\n"));
			g_bThreadRunning = false;
		}
		else if (ID_FILE_DOWNLOAD == nFileEvent)
		{
			CString strMessage;
			strMessage.Format(_T("Downloading %s..."), strFilePath.c_str());
			pMainFrame->ShowMessage(strMessage.GetBuffer(), strFilePath);
			strMessage.ReleaseBuffer();
		}
		else if (ID_FILE_UPLOAD == nFileEvent)
		{
			CString strMessage;
			strMessage.Format(_T("Uploading %s..."), strFilePath.c_str());
			pMainFrame->ShowMessage(strMessage.GetBuffer(), strFilePath);
			strMessage.ReleaseBuffer();
		}
		else if (ID_FILE_DELETE == nFileEvent)
		{
			CString strMessage;
			strMessage.Format(_T("Deleting %s..."), strFilePath.c_str());
			pMainFrame->ShowMessage(strMessage.GetBuffer(), strFilePath);
			strMessage.ReleaseBuffer();
		}

		ReleaseSemaphore(hResourceMutex, 1, nullptr);
		ReleaseSemaphore(hEmptySemaphore, 1, nullptr);

		while (g_bThreadRunning && !g_bIsConnected);

		WaitForSingleObject(hSocketMutex, INFINITE);

		try
		{
			if (pApplicationSocket.IsWritable(1000))
			{
				if (ID_STOP_PROCESS == nFileEvent)
				{
					const std::string strCommand = "Close";
					nLength = (int)strCommand.length() + 1;
					if (WriteBuffer(pApplicationSocket, (unsigned char*) strCommand.c_str(), nLength, true, true))
					{
						TRACE("Closing...\n");
						pApplicationSocket.Close();
					}
				}
				else
				{
					if (ID_FILE_DOWNLOAD == nFileEvent)
					{
						const std::string strCommand = "Download";
						nLength = (int)strCommand.length() + 1;
						if (WriteBuffer(pApplicationSocket, (unsigned char*)strCommand.c_str(), nLength, true, false))
						{
							const std::string strASCII = wstring_to_utf8(encode_filepath(strFilePath));
							const int nFileNameLength = (int)strASCII.length() + 1;
							if (WriteBuffer(pApplicationSocket, (unsigned char*)strASCII.c_str(), nFileNameLength, false, false))
							{
								TRACE(_T("Downloading %s...\n"), strFilePath.c_str());
								VERIFY(DownloadFile(pApplicationSocket, strFilePath));
							}
						}
					}
					else
					{
						if (ID_FILE_UPLOAD == nFileEvent)
						{
							const std::string strCommand = "Upload";
							nLength = (int)strCommand.length() + 1;
							if (WriteBuffer(pApplicationSocket, (unsigned char*) strCommand.c_str(), nLength, true, false))
							{
								const std::string strASCII = wstring_to_utf8(encode_filepath(strFilePath));
								const int nFileNameLength = (int)strASCII.length() + 1;
								if (WriteBuffer(pApplicationSocket, (unsigned char*) strASCII.c_str(), nFileNameLength, false, false))
								{
									TRACE(_T("Uploading %s...\n"), strFilePath.c_str());
									VERIFY(UploadFile(pApplicationSocket, strFilePath));
								}
							}
						}
						else
						{
							if (ID_FILE_DELETE == nFileEvent)
							{
								const std::string strCommand = "Delete";
								nLength = (int)strCommand.length() + 1;
								if (WriteBuffer(pApplicationSocket, (unsigned char*) strCommand.c_str(), nLength, true, false))
								{
									const std::string strASCII = wstring_to_utf8(encode_filepath(strFilePath));
									const int nFileNameLength = (int)strASCII.length() + 1;
									if (WriteBuffer(pApplicationSocket, (unsigned char*) strASCII.c_str(), nFileNameLength, false, true))
									{
										TRACE(_T("Deleting %s...\n"), strFilePath.c_str());
									}
								}
							}
						}
					}
				}
			}
		}
		catch (CWSocketException* pException)
		{
			const int nErrorLength = 0x100;
			TCHAR lpszErrorMessage[nErrorLength] = { 0, };
			pException->GetErrorMessage(lpszErrorMessage, nErrorLength);
			TRACE(_T("%s\n"), lpszErrorMessage);
			pException->Delete();
			g_bIsConnected = false;
		}

		ReleaseSemaphore(hSocketMutex, 1, nullptr);
	}
	return 0;
}

void AddNewItem(const int nFileEvent, const std::wstring& strFilePath, LPVOID lpParam)
{
	if ((g_strCurrentDocument.compare(strFilePath) == 0) && (ID_FILE_UPLOAD == nFileEvent))
	{
		return;
	}

	CMainFrame* pMainFrame = (CMainFrame*)lpParam;
	HANDLE& hOccupiedSemaphore = pMainFrame->m_hOccupiedSemaphore;
	HANDLE& hEmptySemaphore = pMainFrame->m_hEmptySemaphore;
	HANDLE& hResourceMutex = pMainFrame->m_hResourceMutex;

	WaitForSingleObject(hEmptySemaphore, INFINITE);
	WaitForSingleObject(hResourceMutex, INFINITE);

	TRACE(_T("[AddNewItem] nFileEvent = %d, strFilePath = \"%s\"\n"), nFileEvent, strFilePath.c_str());
	pMainFrame->m_pResourceArray[pMainFrame->m_nNextIn].nFileEvent = nFileEvent;
	pMainFrame->m_pResourceArray[pMainFrame->m_nNextIn].strFilePath = strFilePath;
	pMainFrame->m_nNextIn++;
	pMainFrame->m_nNextIn %= NOTIFY_FILE_SIZE;

	ReleaseSemaphore(hResourceMutex, 1, nullptr);
	ReleaseSemaphore(hOccupiedSemaphore, 1, nullptr);
}
