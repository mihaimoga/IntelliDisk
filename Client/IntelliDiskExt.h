/* This file is part of IntelliDisk application developed by Stefan-Mihai MOGA.

IntelliDisk is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Open
Source Initiative, either version 3 of the License, or any later version.

IntelliDisk is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
IntelliDisk. If not, see <http://www.opensource.org/licenses/gpl-3.0.html>*/

#pragma once

#include "FileInformation.h"
#include "NotifyDirCheck.h"

inline byte calcLRC(byte* buffer, const int length)
{
	byte nLRC = 0;
	for (int i = 0; i < length; nLRC = nLRC ^ buffer[i], i++);
	return nLRC;
}

inline byte calcLRC(const std::vector<byte>& buffer)
{
	byte nLRC = 0;
	for (auto i = buffer.begin(); i != buffer.end(); ++i)
		nLRC = nLRC ^ *i;
	return nLRC;
}

std::wstring utf8_to_wstring(const std::string& str);
std::string wstring_to_utf8(const std::wstring& str);

const std::string GetMachineID();
const std::wstring GetSpecialFolder();

bool InstallStartupApps(bool bInstallStartupApps);

UINT DirCallback(CFileInformation fiObject, EFileAction faAction, LPVOID lpData);
DWORD WINAPI ProducerThread(LPVOID lpParam);
DWORD WINAPI ConsumerThread(LPVOID lpParam);
void AddNewItem(const int nFileEvent, const std::wstring& strFilePath, LPVOID lpParam);
