/* This file is part of IntelliDisk application developed by Mihai MOGA.

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
#include "SocMFC.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define IntelliDiskIP _T("127.0.0.1")
#define IntelliDiskPort 8080

#define STX 0x02
#define ETX 0x03
#define ACK 0x06

constexpr auto BSIZE = 0x10000; // this is only for testing, not for the final commercial application

bool bThreadRunning = false;
CWSocket g_pServerSocket;
int g_nSocketCount = 0;
CWSocket g_pClientSocket[BSIZE];
int g_nThreadCount = 0;
DWORD m_dwThreadID[BSIZE];
HANDLE g_hThreadArray[BSIZE];

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

DWORD WINAPI IntelliDiskThread(LPVOID lpParam)
{
	const int MAX_BUFFER = 0x10000;
	char pBuffer[MAX_BUFFER] = { 0, };
	size_t nLength = 0;
	CWSocket* pApplicationSocket = (CWSocket*)lpParam;
	ASSERT(pApplicationSocket->IsCreated());
	std::wstring strComputerID;

	while (bThreadRunning)
	{
		try
		{
			if (pApplicationSocket->IsReadible(10000))
			{
				ZeroMemory(pBuffer, sizeof(pBuffer));
				if ((pApplicationSocket->Receive(pBuffer, sizeof(pBuffer)) == (strlen("IntelliDisk") + 1)) && (strcmp("IntelliDisk", pBuffer) == 0))
				{
					nLength = 0;
					ZeroMemory(pBuffer, sizeof(pBuffer));
					pBuffer[nLength++] = ACK;
					VERIFY(pApplicationSocket->Send(pBuffer, (int)nLength) == nLength);
					TRACE(_T("Client connected!\n"));
					size_t nComputerLength = 0;
					nLength = sizeof(nComputerLength);
					if ((pApplicationSocket->Receive(&nComputerLength, (int)nLength) == nLength))
					{
						nLength = 0;
						ZeroMemory(pBuffer, sizeof(pBuffer));
						pBuffer[nLength++] = ACK;
						VERIFY(pApplicationSocket->Send(pBuffer, (int)nLength) == nLength);
						nLength = 0;
						ZeroMemory(pBuffer, sizeof(pBuffer));
						while (nLength < nComputerLength)
						{
							const int nReturn = pApplicationSocket->Receive(pBuffer + nLength, (int)(nComputerLength - nLength));
							if (nReturn <= 0)
							{
								Sleep(100);
							}
							else
							{
								nLength += nReturn;
							}
						}
						strComputerID = utf8_to_wstring(pBuffer);
						nLength = 0;
						ZeroMemory(pBuffer, sizeof(pBuffer));
						pBuffer[nLength++] = ACK;
						VERIFY(pApplicationSocket->Send(pBuffer, (int)nLength) == nLength);
					}
					TRACE(_T("Logged In: %s!\n"), strComputerID.c_str());
				}
				else if (strcmp("Close", pBuffer) == 0)
				{
					nLength = 0;
					ZeroMemory(pBuffer, sizeof(pBuffer));
					pBuffer[nLength++] = ACK;
					VERIFY(pApplicationSocket->Send(pBuffer, (int)nLength) == nLength);
					TRACE(_T("Logged Out: %s!\n"), strComputerID.c_str());
					break;
				}
				else if (strcmp("Ping", pBuffer) == 0)
				{
					nLength = 0;
					ZeroMemory(pBuffer, sizeof(pBuffer));
					pBuffer[nLength++] = ACK;
					VERIFY(pApplicationSocket->Send(pBuffer, (int)nLength) == nLength);
					TRACE(_T("Ping!\n"));
				}
				else if (strcmp("Download", pBuffer) == 0)
				{
				}
				else if (strcmp("Upload", pBuffer) == 0)
				{
				}
				else if (strcmp("Delete", pBuffer) == 0)
				{
					nLength = 0;
					ZeroMemory(pBuffer, sizeof(pBuffer));
					pBuffer[nLength++] = ACK;
					VERIFY(pApplicationSocket->Send(pBuffer, (int)nLength) == nLength);
					size_t nFileNameLength = 0;
					nLength = sizeof(nFileNameLength);
					if ((pApplicationSocket->Receive(&nFileNameLength, (int)nLength) == nLength))
					{
						nLength = 0;
						ZeroMemory(pBuffer, sizeof(pBuffer));
						while (nLength < nFileNameLength)
						{
							const int nReturn = pApplicationSocket->Receive(pBuffer + nLength, (int)(nFileNameLength - nLength));
							if (nReturn <= 0)
							{
								Sleep(100);
							}
							else
							{
								nLength += nReturn;
							}
						}
						const std::wstring& strFilePath = utf8_to_wstring(pBuffer);
						nLength = 0;
						ZeroMemory(pBuffer, sizeof(pBuffer));
						pBuffer[nLength++] = ACK;
						VERIFY(pApplicationSocket->Send(pBuffer, (int)nLength) == nLength);
						TRACE(_T("Deleting %s...\n"), strFilePath.c_str());
					}
				}
			}
		}
		catch (CWSocketException* pException)
		{
			TCHAR lpszErrorMessage[0x100] = { 0, };
			pException->GetErrorMessage(lpszErrorMessage, sizeof(lpszErrorMessage));
			TRACE(_T("%s\n"), lpszErrorMessage);
			delete pException;
			pException = NULL;
			TRACE(_T("exiting...\n"));
			return 0;
		}
	}
	pApplicationSocket->Close();
	TRACE(_T("exiting...\n"));
	return 0;
}

void StartProcessingThread()
{
	TRACE(_T("StartProcessingThread()\n"));
	g_pServerSocket.CreateAndBind(IntelliDiskPort, SOCK_STREAM, AF_INET);
	if (bThreadRunning = g_pServerSocket.IsCreated())
	{
		g_pServerSocket.Listen(BSIZE);
		while (bThreadRunning)
		{
			g_pServerSocket.Accept(g_pClientSocket[g_nSocketCount]);
			g_hThreadArray[g_nThreadCount] = CreateThread(NULL, 0, IntelliDiskThread, &g_pClientSocket[g_nSocketCount], 0, &m_dwThreadID[g_nThreadCount]);
			ASSERT(g_hThreadArray[g_nThreadCount] != NULL);
			g_nSocketCount++;
			g_nThreadCount++;
		}
	}
}

void StopProcessingThread()
{
	TRACE(_T("StopProcessingThread()\n"));
	if (bThreadRunning)
	{
		bThreadRunning = false;
		WaitForMultipleObjects(g_nThreadCount, g_hThreadArray, TRUE, INFINITE);
	}
}
