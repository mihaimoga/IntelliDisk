/* Copyright (C) 2022-2024 Stefan-Mihai MOGA
This file is part of IntelliDisk application developed by Stefan-Mihai MOGA.

IntelliDisk is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Open
Source Initiative, either version 3 of the License, or any later version.

IntelliDisk is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
IntelliDisk. If not, see <http://www.opensource.org/licenses/gpl-3.0.html>*/

#pragma once

#ifndef __INTELLIDISK_EXT__
#define __INTELLIDISK_EXT__

#include "FileInformation.h"
#include "NotifyDirCheck.h"
#include "SocMFC.h"

inline unsigned char calcLRC(const unsigned char* buffer, const int length)
{
	unsigned char nLRC = 0;
	for (int i = 0; i < length; nLRC = nLRC ^ buffer[i], i++);
	return nLRC;
}

inline unsigned char calcLRC(const std::vector<unsigned char>& buffer)
{
	unsigned char nLRC = 0;
	for (auto i = buffer.begin(); i != buffer.end(); ++i)
		nLRC = nLRC ^ *i;
	return nLRC;
}

std::wstring utf8_to_wstring(const std::string& str);
std::string wstring_to_utf8(const std::wstring& str);

const std::string GetMachineID();
const std::wstring GetSpecialFolder();

bool InstallStartupApps(bool bInstallStartupApps);

bool ReadBuffer(CWSocket& pApplicationSocket, unsigned char* pBuffer, int& nLength, const bool ReceiveENQ, const bool ReceiveEOT);
bool WriteBuffer(CWSocket& pApplicationSocket, const unsigned char* pBuffer, const int nLength, const bool SendENQ, const bool SendEOT);

bool DownloadFile(CWSocket& pApplicationSocket, const std::wstring& strFilePath);
bool UploadFile(CWSocket& pApplicationSocket, const std::wstring& strFilePath);

UINT DirCallback(CFileInformation fiObject, EFileAction faAction, LPVOID lpData);
DWORD WINAPI ProducerThread(LPVOID lpParam);
DWORD WINAPI ConsumerThread(LPVOID lpParam);
void AddNewItem(const int nFileEvent, const std::wstring& strFilePath, LPVOID lpParam);

#endif
