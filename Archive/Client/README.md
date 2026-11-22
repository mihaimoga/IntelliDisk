# IntelliDisk Client

The **IntelliDisk Client** is part of the IntelliDisk distributed storage system—a free and lightweight alternative to cloud storage solutions like Microsoft OneDrive. It is designed to run on Windows and communicates with a remote server to synchronize files across devices.

## 🧩 Overview

IntelliDisk uses a **client-server architecture**:
- The **Client** handles user interaction and file synchronization.
- The **Server** manages storage and database operations using MySQL.

This repository contains the **Client-side application**, written in **C++** using **Win32 API** and **STL**, optimized for performance and minimal resource usage.

## 🚀 Features

- Sync files across multiple devices
- Lightweight and fast execution
- Low power consumption for greener computing
- Easy access to shared cloud-like storage
- Auto-start on Windows login
- Configurable server IP and port

## 🛠️ Installation

### Option 1: Installer

1. Download the installer:
   - [IntelliDiskSetup.msi](https://www.moga.doctor/freeware/IntelliDiskSetup.msi).
2. Follow the installation wizard.

### Option 2: Portable ZIP

1. Download the ZIP:
   - [IntelliDisk.zip](https://www.moga.doctor/freeware/IntelliDisk.zip).
2. Unzip the content into the new folder.
3. Run `IntelliDisk.exe` from the extracted folder.

## ⚙️ Configuration

### Client Setup

1. Open the settings panel.
2. Set the **Server IP** and **Port**.
3. Enable **Auto-start on Windows login** (optional).

### Server Requirements
- Install [MySQL ODBC Connector](https://dev.mysql.com/downloads/connector/odbc/)
- Choose a MySQL hosting service and create the MySQL database;
- Configure Server instance (create `IntelliDisk.xml` configuration file):
```xml
<?xml version="1.0" encoding="UTF-16" standalone="no"?>
<xml>
    <IntelliDisk>
        <ServicePort>8080</ServicePort>
        <HostName>localhost</HostName>
        <HostPort>3306</HostPort>
        <Database>MySQL_database</Database>
        <Username>MySQL_username</Username>
        <Password>MySQL_password</Password>
    </IntelliDisk>
</xml>
```

## 📦 Dependencies
The client uses several open-source components:
- [IAppSettings](https://www.naughter.com/appsettings.html)
- [base64](https://github.com/ReneNyffenegger/cpp-base64)
- [genUp4win](https://github.com/mihaimoga/genUp4win)
- [SHA256](https://github.com/System-Glitch/SHA256)
- [CTrayNotifyIcon](https://www.naughter.com/ntray.html)
- [CVersionInfo](https://www.naughter.com/versioninfo.html)
- [CWSocket](https://www.naughter.com/w3mfc.html)

## 🤝 Contributing
Please follow the Contributing Rules:
- Create a GitHub issue describing your proposed change.
- Submit a Pull Request referencing the issue.
- Respond to feedback from maintainers.

## 📄 License
This project is licensed under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html).
