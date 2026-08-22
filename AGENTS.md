# AGENTS.md

Welcome to the **IntelliDisk** repository. This document provides essential context, architectural overviews, conventions, and guidelines for AI coding agents and human developers working on this codebase.

---

## 1. Project Overview

**IntelliDisk** is a distributed storage and synchronization solution for Microsoft Windows. It enables users to sync files across multiple devices via a client-server architecture using pure Win32 API, MFC (Microsoft Foundation Classes), and standard C++ STL for high performance, small binary footprint, and low power consumption.

### Solution Projects

| Project | Location | Description |
| :--- | :--- | :--- |
| **IntelliDisk** (Client) | `Client\IntelliDisk.vcxproj` | MFC-based desktop client with system tray integration, Edge WebView2 support, file-system change monitoring, SHA-256 hashing, and socket synchronization. |
| **IntelliHost** (Server) | `Server\IntelliHost.vcxproj` | Windows NT Service backend managing MySQL data storage through ODBC wrappers and socket communications. |
| **QuickTest** (Test Utility) | `Server\Test\QuickTest.vcxproj` | MFC dialog-based diagnostic tool for verifying MySQL database connectivity, queries, and server credentials. |
| **genUp4win** (Auto-Updater) | `genUp4win\genUp4win.vcxproj` | Shared library / DLL providing software update checking and download capabilities. |

---

## 2. Technology Stack & Dependencies

- **Language & Frameworks**: C++ (C++17/C++20), Win32 API, MFC, Standard Template Library (STL).
- **Database Layer**: MySQL via ODBC 8.0/9.0 Driver (`ODBCWrappers.h`, `IntelliDiskSQL.h`).
- **Networking**: TCP Sockets (`SocMFC.h`, `SocMFC.cpp`).
- **Web Component**: Microsoft Edge WebView2 (`EdgeWebBrowser.h`), Microsoft Windows Implementation Library (WIL).
- **Security & Integrity**: SHA-256 cryptographic hashing (`SHA256.h`), Base64 encoding/decoding (`base64.h`).
- **Build System**: Visual Studio Solution (`.sln`), MSBuild, and optional CMake/Conan for `genUp4win`.

---

## 3. Build & Development Commands

### Building with MSBuild / Visual Studio

Build the solution across target configurations (`Debug`/`Release`) and platforms (`x64`/`Win32`):

```cmd
:: Build entire solution (Release x64)
msbuild IntelliDisk.sln /p:Configuration=Release /p:Platform=x64

:: Build entire solution (Debug x64)
msbuild IntelliDisk.sln /p:Configuration=Debug /p:Platform=x64
```

### Database Setup
The server requires a configured MySQL database instance:
1. Install [MySQL ODBC Connector](https://dev.mysql.com/downloads/connector/odbc/).
2. Initialize tables using `IntelliDisk.sql`:
   ```sql
   DROP TABLE IF EXISTS `filedata`;
   DROP TABLE IF EXISTS `filename`;
   CREATE TABLE `filename` (`filename_id` BIGINT NOT NULL AUTO_INCREMENT, `filepath` VARCHAR(256) NOT NULL, `filesize` BIGINT NOT NULL, PRIMARY KEY(`filename_id`)) ENGINE=InnoDB;
   CREATE TABLE `filedata` (`filedata_id` BIGINT NOT NULL AUTO_INCREMENT, `filename_id` BIGINT NOT NULL, `content` LONGTEXT NOT NULL, `base64` BIGINT NOT NULL, PRIMARY KEY(`filedata_id`), FOREIGN KEY filedata_fk(filename_id) REFERENCES filename(filename_id)) ENGINE=InnoDB;
   CREATE UNIQUE INDEX index_filepath ON `filename`(`filepath`);
   ```
3. Configure `IntelliDisk.xml` for server connection settings (ServicePort, HostName, HostPort, Database, Username, Password).

---

## 4. Coding Standards & Conventions

All contributions and AI-generated modifications must adhere to the project's coding rules defined in `CONTRIBUTING.md`:

### Formatting & Braces
- **Allman Style Braces**: Place opening and closing braces on separate lines for functions, classes, and control flow blocks.
  ```cpp
  void MyClass::method()
  {
	  if (condition)
	  {
		  // Action
	  }
  }
  ```
- **Inline Header Methods**: Single-line method definitions in header files (`.h`) may use same-line braces:
  ```cpp
  int getCount() { return _count; }
  ```
- **Tabs**: Use tabs for indentation.
- **Operator Spacing**: Always place spaces around binary and ternary operators (`a == 10 && b == 42`).
- **Control Flow Spacing**: Place a space between control keywords and parentheses (`if (condition)`, `while (loop)`), and after semicolons in `for` loops (`for (int i = 0; i < 10; ++i)`).

### Naming Conventions
- **Classes & Structs**: PascalCase (e.g., `CChildView`, `CServiceInstaller`).
- **Methods & Parameters**: camelCase (e.g., `processRequest(uint messageId)`).
- **Member Variables**: Preceded by `_` or MFC standard `m_` (e.g., `_publicAttribute`, `m_hWnd`).

### C++ Best Practices
- **String Checks**: Use `.empty()` / `IsEmpty()` instead of comparison against empty string `""`.
- **Casts**: Avoid C-style casts `(Type)var`; always prefer C++ explicit casts like `static_cast<Type>(var)` or `reinterpret_cast<Type>(var)`.
- **Constants**: Prefer enums or `constexpr` over magic numbers.
- **Resource Management**: Use RAII principles for Win32 handles, memory, and database connections.

---

## 5. Key Architecture & File Layout

- `Client/`
  - `IntelliDisk.cpp`, `MainFrame.cpp`, `ChildView.cpp`: Client application life cycle, ribbon, and main UI.
  - `NotifyDirCheck.cpp`, `FileInformation.cpp`: File monitoring and change detection engine.
  - `EdgeWebBrowser.cpp`, `WebBrowserDlg.cpp`: Embedded WebView2 web browser component.
  - `NTray.cpp`: System notification tray icon management.
  - `SettingsDlg.cpp`, `CheckForUpdatesDlg.cpp`: Client settings and updater dialogs.
- `Server/`
  - `IntelliDisk.cpp`, `ServiceBase.cpp`, `ServiceInstaller.cpp`: Windows NT service control and dispatcher.
  - `IntelliDiskSQL.cpp`, `ODBCWrappers.h`: Database interaction, query execution, and records mapping.
  - `IntelliDiskINI.cpp`, `AppSettings.h`: XML/INI configuration loader and persistence.
  - `SocMFC.cpp`: Multithreaded TCP socket server.
- `Server/Test/`
  - `QuickTest.cpp`, `QuickTestDlg.cpp`: Interactive GUI test tool for database verification.
- `genUp4win/`
  - `genUp4win.cpp`: Dynamic updater library and version checking API.

---

## 6. Guidelines for AI Agents

1. **Minimal, Targeted Changes**: Do not perform widespread reformatting, whitespace adjustments, or unnecessary refactoring.
2. **Preserve Compatibility**: Maintain binary/API compatibility for Win32/MFC interfaces and existing socket protocols.
3. **Validate Build**: Always ensure the workspace builds cleanly using MSBuild without warnings or errors.
4. **Test Coverage**: When altering server SQL operations or socket communications, verify compatibility with both `QuickTest` and the Client application.
