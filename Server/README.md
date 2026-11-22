# IntelliDisk Server

The **IntelliDisk Server** is the backend component of the IntelliDisk distributed storage system—a lightweight, open-source alternative to cloud storage platforms like Microsoft OneDrive. It handles file storage, user authentication, and database operations.

## 🧩 Overview
The server is written in **C++** and uses **ODBC** to connect to a **MySQL** database. It listens for client requests over TCP/IP and processes file synchronization, user management, and metadata operations.

## 📁 Project Structure
- `IntelliDiskServer.cpp` – Main server logic and TCP listener
- `IntelliDisk.xml` – Configuration file for database credentials
- `ODBCWrappers` – Lightweight C++ wrappers for ODBC database access
- `SHA256`, `base64` – Utility libraries for hashing and encoding
- `CWSocket` – TCP socket communication layer

## ⚙️ Requirements

- Windows OS
- MySQL Server
- [MySQL ODBC Connector](https://dev.mysql.com/downloads/connector/)

## 🛠️ Setup Instructions

### 1. Configure MySQL

Create a MySQL database and user with appropriate permissions. Then, update the `IntelliDisk.xml` file:

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

### 2. Build the Server
Use Visual Studio or another C++ IDE to open the project and build the executable.

### 3. Run the Server
Launch the server executable. It will listen for incoming client connections on the configured port.

## 🔐 Security
- Passwords are hashed using SHA-256
- Communication is over TCP/IP (I'm considering adding TLS for production)
- Base64 encoding is used for file data transmission

## 📦 Dependencies
The server uses several open-source components:
- [base64](https://github.com/ReneNyffenegger/cpp-base64)
- [ODBCWrappers](https://www.naughter.com/odbcwrappers.html)
- [SHA256](https://github.com/System-Glitch/SHA256)
- [CWSocket](https://www.naughter.com/w3mfc.html)

## 🧪 Testing
You can test the server using the IntelliDisk Client or by simulating TCP requests with tools like:
- Postman
- netcat
- Custom C++ test clients

## 🤝 Contributing
Please follow the Contributing Rules:
- Create a GitHub issue describing your proposed change.
- Submit a Pull Request referencing the issue.
- Respond to feedback from maintainers.

## 📄 License
This project is licensed under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html).
