/* Copyright (C) 2022-2026 Stefan-Mihai MOGA
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

/**
 * @file IntelliDisk.cpp
 * @brief Main entry point for the IntelliDisk Windows service application
 * @details This file contains the wmain function that handles service installation,
 *          removal, and execution of the IntelliDisk service
 */

#include "pch.h"
#include "framework.h"

#include "IntelliDisk.h"
#include "ServiceInstaller.h"
#include "ServiceBase.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// === SERVICE CONFIGURATION ===
// These constants define how the service is registered with Windows Service Control Manager (SCM)

// Internal name of the service (used by SCM and command-line tools)
const wchar_t* SERVICE_NAME = L"IntelliDisk";

// Displayed name of the service (shown in Services MMC snap-in)
const wchar_t* SERVICE_DISPLAY_NAME = L"IntelliDisk Service";

// Service start options:
// - SERVICE_AUTO_START: Starts automatically at system boot
// - SERVICE_DEMAND_START: Starts manually or on demand
// - SERVICE_DISABLED: Cannot be started
#define SERVICE_START_TYPE       SERVICE_DEMAND_START

// List of service dependencies (services that must start before this one)
// Format: "dep1\0dep2\0\0" (double-null terminated)
// Empty string = no dependencies
#define SERVICE_DEPENDENCIES     L""

// The name of the account under which the service should run
// LocalService: Limited privileges, network access with anonymous credentials
// Other options: LocalSystem, NetworkService, or custom domain account
#define SERVICE_ACCOUNT          L"NT AUTHORITY\\LocalService"

// The password to the service account name
// NULL for built-in accounts (LocalService, LocalSystem, NetworkService)
#define SERVICE_PASSWORD         NULL

/**
 * @brief Main entry point for the IntelliDisk service application
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return 0 on successful execution
 * @details Processes command-line arguments:
 *          - "-install" or "/install": Installs the IntelliDisk service
 *          - "-remove" or "/remove": Uninstalls the IntelliDisk service
 *          - No arguments or invalid arguments: Displays help or runs the service
 * 
 * COMMAND-LINE USAGE:
 * ===================
 * IntelliDisk.exe -install    // Install service (requires admin privileges)
 * IntelliDisk.exe -remove     // Uninstall service (requires admin privileges)
 * IntelliDisk.exe             // Run as console app or started by SCM
 * 
 * DEPLOYMENT SCENARIOS:
 * =====================
 * 1. Console Mode: Run without arguments for help or testing
 * 2. Service Mode: Started by Windows Service Control Manager
 * 3. Installation: Use -install to register with SCM
 * 4. Uninstallation: Use -remove to unregister from SCM
 */
int wmain(int argc, wchar_t *argv[])
{
	// Check if command-line argument provided and starts with '-' or '/'
	// Standard Windows convention: both '-' and '/' are valid switch prefixes
	if ((argc > 1) && ((*argv[1] == L'-' || (*argv[1] == L'/'))))
	{
		// === SERVICE INSTALLATION ===
		// Case-insensitive comparison of command (ignoring the prefix character)
		if (_wcsicmp(L"install", argv[1] + 1) == 0)
		{
			// Install the service when the command is 
			// "-install" or "/install".
			// Requires administrator privileges
			InstallService(
				(PWSTR)SERVICE_NAME,               // Name of service (used by SCM)
				(PWSTR)SERVICE_DISPLAY_NAME,       // Name to display in Services GUI
				SERVICE_START_TYPE,                // Manual start (demand start)
				(PWSTR)SERVICE_DEPENDENCIES,       // No dependencies
				(PWSTR)SERVICE_ACCOUNT,            // Run as LocalService account
				SERVICE_PASSWORD                   // NULL for built-in accounts
			);
		}
		// === SERVICE REMOVAL ===
		else if (_wcsicmp(L"remove", argv[1] + 1) == 0)
		{
			// Uninstall the service when the command is 
			// "-remove" or "/remove".
			// Requires administrator privileges
			// Will stop the service if running before removal
			UninstallService((PWSTR)SERVICE_NAME);
		}
	}
	else
	{
		// === NO VALID ARGUMENTS - SHOW HELP OR RUN SERVICE ===
		// This block executes in two scenarios:
		// 1. User runs from command line without arguments (console mode)
		// 2. Windows SCM starts the service (service mode)

		// Display help message (only visible in console mode)
		wprintf(L"Parameters:\n");
		wprintf(L" -install  to install the service.\n");
		wprintf(L" -remove   to remove the service.\n");

		// Create service instance
		CServiceBase service((PWSTR)SERVICE_NAME);

		// Run the service
		// - If started by SCM: Enters service mode and processes SCM commands
		// - If started from console: Attempts to connect to SCM (will fail)
		if (!CServiceBase::Run(service))
		{
			// Service failed to start (e.g., insufficient privileges, already running)
			wprintf(L"Service failed to run w/err 0x%08lx\n", GetLastError());
		}
	}

	return 0;
}
