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
#include "SocMFC.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define IntelliDiskIP _T("127.0.0.1")
#define IntelliDiskPort 8080

#define STX 0x02
#define ETX 0x03
#define ACK 0x06
#define NAK 0x15

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

const int MAX_BUFFER = 0x10000;

bool ReadBuffer(CWSocket* pApplicationSocket, char* pBuffer, int& nLength)
{
	int nCount = 0;
	char nReturn = ACK;

	try
	{
		do {
			if ((nLength = pApplicationSocket->Receive(pBuffer, nLength)) > 0)
			{
				nReturn = (pBuffer[nLength - 1] == calcLRC((byte*)&pBuffer[3], (nLength - 5))) ? ACK : NAK;
				VERIFY(pApplicationSocket->Send(&nReturn, sizeof(nReturn)) == 1);
			}
			else
				return false;
		} while ((nReturn != ACK) && (++nCount <= 3));
	}
	catch (CWSocketException* pException)
	{
		TCHAR lpszErrorMessage[0x100] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, sizeof(lpszErrorMessage));
		TRACE(_T("%s\n"), lpszErrorMessage);
		delete pException;
		pException = NULL;
		return false;
	}
	return true;
}

bool WriteBuffer(CWSocket* pApplicationSocket, const char* pBuffer, int nLength)
{
	int nCount = 0;
	char nReturn = ACK;
	char pPacket[MAX_BUFFER] = { 0, };

	try
	{
		ASSERT(nLength < sizeof(pPacket));
		ZeroMemory(pPacket, sizeof(pPacket));
		CopyMemory(&pPacket[3], pBuffer, nLength);
		pPacket[0] = STX;
		pPacket[1] = nLength / 0x100;
		pPacket[2] = nLength % 0x100;
		pPacket[3 + nLength] = ETX;
		pPacket[4 + nLength] = calcLRC((byte*)pBuffer, nLength);
		do {
			if (pApplicationSocket->Send(pPacket, (5 + nLength)) == (5 + nLength))
			{
				VERIFY(pApplicationSocket->Receive(&nReturn, sizeof(nReturn)) == 1);
			}
		} while ((nReturn != ACK) && (++nCount <= 3));
	}
	catch (CWSocketException* pException)
	{
		TCHAR lpszErrorMessage[0x100] = { 0, };
		pException->GetErrorMessage(lpszErrorMessage, sizeof(lpszErrorMessage));
		TRACE(_T("%s\n"), lpszErrorMessage);
		delete pException;
		pException = NULL;
		return false;
	}
	return (nReturn == ACK);
}

DWORD WINAPI IntelliDiskThread(LPVOID lpParam)
{
	char pBuffer[MAX_BUFFER] = { 0, };
	int nLength = 0;
	CWSocket* pApplicationSocket = (CWSocket*)lpParam;
	ASSERT(pApplicationSocket->IsCreated());
	std::wstring strComputerID;

	while (bThreadRunning)
	{
		try
		{
			if (pApplicationSocket->IsReadible(10000))
			{
				nLength = sizeof(pBuffer);
				ZeroMemory(pBuffer, sizeof(pBuffer));
				if (ReadBuffer(pApplicationSocket, pBuffer, nLength))
				{
					const std::string strCommand = &pBuffer[3];
					if (strCommand.compare("IntelliDisk") == 0)
					{
						TRACE(_T("Client connected!\n"));
						nLength = sizeof(pBuffer);
						ZeroMemory(pBuffer, sizeof(pBuffer));
						if (ReadBuffer(pApplicationSocket, pBuffer, nLength))
						{
							const std::string strCommand = &pBuffer[3];
							strComputerID = utf8_to_wstring(strCommand);
							TRACE(_T("Logged In: %s!\n"), strComputerID.c_str());
						}
					}
					else
					{
						if (strCommand.compare("Close") == 0)
						{
							TRACE(_T("Logged Out: %s!\n"), strComputerID.c_str());
							break;
						}
						else
						{
							if (strCommand.compare("Ping") == 0)
							{
								TRACE(_T("Ping!\n"));
							}
							else
							{
								if (strCommand.compare("Download") == 0)
								{
									nLength = sizeof(pBuffer);
									ZeroMemory(pBuffer, sizeof(pBuffer));
									if (ReadBuffer(pApplicationSocket, pBuffer, nLength))
									{
										const std::string strCommand = &pBuffer[3];
										const std::wstring& strFilePath = utf8_to_wstring(strCommand);
										TRACE(_T("Downloading %s...\n"), strFilePath.c_str());
									}
								}
								else
								{
									if (strCommand.compare("Upload") == 0)
									{
										nLength = sizeof(pBuffer);
										ZeroMemory(pBuffer, sizeof(pBuffer));
										if (ReadBuffer(pApplicationSocket, pBuffer, nLength))
										{
											const std::string strCommand = &pBuffer[3];
											const std::wstring& strFilePath = utf8_to_wstring(strCommand);
											TRACE(_T("Uploading %s...\n"), strFilePath.c_str());
										}
									}
									else
									{
										if (strCommand.compare("Delete") == 0)
										{
											nLength = sizeof(pBuffer);
											ZeroMemory(pBuffer, sizeof(pBuffer));
											if (ReadBuffer(pApplicationSocket, pBuffer, nLength))
											{
												const std::string strCommand = &pBuffer[3];
												const std::wstring& strFilePath = utf8_to_wstring(strCommand);
												TRACE(_T("Deleting %s...\n"), strFilePath.c_str());
											}
										}
									}
								}
							}
						}
					}
				}
			}
			else
			{
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
