/* Copyright (C) 2022-2025 Stefan-Mihai MOGA
This file is part of IntelliDisk application developed by Stefan-Mihai MOGA.
IntelliDisk is an alternative Windows version to the famous Microsoft OneDrive!

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

/**
 * @brief Calculates the Longitudinal Redundancy Check (LRC) for a buffer.
 * @param buffer Pointer to the buffer.
 * @param length Number of bytes to process.
 * @return The computed LRC value.
 */
inline unsigned char calcLRC(const unsigned char* buffer, const int length)
{
	unsigned char nLRC = 0;
	for (int i = 0; i < length; nLRC = nLRC ^ buffer[i], i++);
	return nLRC;
}

/**
 * @brief Calculates the LRC for a vector of bytes.
 * @param buffer Vector of bytes.
 * @return The computed LRC value.
 */
inline unsigned char calcLRC(const std::vector<unsigned char>& buffer)
{
	unsigned char nLRC = 0;
	for (auto i = buffer.begin(); i != buffer.end(); ++i)
		nLRC = nLRC ^ *i;
	return nLRC;
}

/**
 * @brief Converts a UTF-8 encoded std::string to std::wstring.
 * @param str UTF-8 encoded string.
 * @return Wide string representation.
 */
std::wstring utf8_to_wstring(const std::string& str);

/**
 * @brief Converts a std::wstring to a UTF-8 encoded std::string.
 * @param str Wide string.
 * @return UTF-8 encoded string.
 */
std::string wstring_to_utf8(const std::wstring& str);

/**
 * @brief Retrieves a unique machine identifier (user and computer name).
 * @return UTF-8 encoded string in the format "username:computername".
 */
const std::string GetMachineID();

/**
 * @brief Gets the path to the user's profile folder with "IntelliDisk\" appended.
 * @return Wide string with the special folder path.
 */
const std::wstring GetSpecialFolder();

/**
 * @brief Installs or removes IntelliDisk from Windows startup applications.
 * @param bInstallStartupApps If true, adds to startup; if false, removes.
 * @return true on success, false otherwise.
 */
bool InstallStartupApps(bool bInstallStartupApps);

/**
 * @brief Reads a buffer from the socket, handling protocol handshakes.
 * @param pApplicationSocket The socket to read from.
 * @param pBuffer Buffer to store received data.
 * @param nLength [in/out] Length of buffer and received data.
 * @param ReceiveENQ Whether to expect an ENQ handshake.
 * @param ReceiveEOT Whether to expect an EOT handshake.
 * @return true on successful read and protocol validation, false otherwise.
 */
bool ReadBuffer(CWSocket& pApplicationSocket, unsigned char* pBuffer, int& nLength, const bool ReceiveENQ, const bool ReceiveEOT);

/**
 * @brief Writes a buffer to the socket, handling protocol handshakes.
 * @param pApplicationSocket The socket to write to.
 * @param pBuffer Buffer containing data to send.
 * @param nLength Length of data to send.
 * @param SendENQ Whether to send an ENQ handshake.
 * @param SendEOT Whether to send an EOT handshake.
 * @return true on successful write and protocol validation, false otherwise.
 */
bool WriteBuffer(CWSocket& pApplicationSocket, const unsigned char* pBuffer, const int nLength, const bool SendENQ, const bool SendEOT);

/**
 * @brief Downloads a file from the server using the application socket.
 *        Verifies file integrity using SHA256.
 * @param pApplicationSocket The socket to use.
 * @param strFilePath The local file path to save to.
 * @return true on success, false otherwise.
 */
bool DownloadFile(CWSocket& pApplicationSocket, const std::wstring& strFilePath);

/**
 * @brief Uploads a file to the server using the application socket.
 *        Sends file data and SHA256 digest for integrity verification.
 * @param pApplicationSocket The socket to use.
 * @param strFilePath The local file path to upload.
 * @return true on success, false otherwise.
 */
bool UploadFile(CWSocket& pApplicationSocket, const std::wstring& strFilePath);

/**
 * @brief Directory monitoring callback for file events.
 *        Adds a new item to the processing queue based on the file action.
 * @param fiObject File information object.
 * @param faAction File action (create, delete, etc).
 * @param lpData User data pointer.
 * @return 0 on success.
 */
UINT DirCallback(CFileInformation fiObject, EFileAction faAction, LPVOID lpData);

/**
 * @brief Producer thread function.
 *        Handles connection establishment, login, and incoming server commands.
 * @param lpParam Pointer to CMainFrame instance.
 * @return 0 on thread exit.
 */
DWORD WINAPI ProducerThread(LPVOID lpParam);

/**
 * @brief Consumer thread function.
 *        Processes file events (upload, download, delete) from the resource queue.
 * @param lpParam Pointer to CMainFrame instance.
 * @return 0 on thread exit.
 */
DWORD WINAPI ConsumerThread(LPVOID lpParam);

/**
 * @brief Adds a new file event item to the resource queue for processing.
 * @param nFileEvent The file event type (upload, download, delete, etc.).
 * @param strFilePath The file path associated with the event.
 * @param lpParam Pointer to CMainFrame instance.
 */
void AddNewItem(const int nFileEvent, const std::wstring& strFilePath, LPVOID lpParam);

#endif
