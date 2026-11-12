# Time Traveller Debugger — Phase 02

A C++ based network-enabled debugging system that allows users to upload source programs, execute them remotely, and navigate through their execution history.

## Overview

Time Traveller Debugger Phase 02 extends the original Phase 01 debugger by introducing a network-based client-server architecture using TCP socket programming.

The system enables a client to upload a source file to a backend server through a Load Balancer. The backend processes the uploaded program using the Phase 01 debugger, generates an execution history file (`.tdbg`), and sends the result back to the client.

## System Architecture

```text
             CLIENT
                |
                | TCP Connection
                |
          LOAD BALANCER
             Port 5000
                |
                | TCP Connection
                |
          BACKEND SERVER
             Port 5001
                |
                |
       PHASE 01 DEBUGGER
                |
                |
        Execution History
           (.tdbg file)
                |
                |
          CLIENT OUTPUT
```

## Key Features

- **Client-Server Communication:** TCP-based communication between the client and backend server.
- **Load Balancer:** Handles incoming client connections and forwards data to the backend.
- **Source File Upload:** Transfers source programs from the client to the backend.
- **Data Integrity:** Uses CRC32 checksums to verify uploaded data.
- **Remote Execution:** Executes uploaded programs using the Phase 01 debugger.
- **Execution History:** Generates and transfers `.tdbg` files containing execution snapshots.
- **Time Travel Navigation:** Allows users to navigate through program execution history.
- **Error Handling:** Handles file operations, network failures, and invalid uploads.

## Technologies Used

- C++
- C++17
- TCP/IP Socket Programming
- POSIX Sockets
- CRC32 Checksum
- Linux / WSL
- Pthreads

## Project Structure

```text
Time-Traveller-Debugger-_Phase02/
│
├── client.cpp
├── loadbalancer.cpp
├── server.cpp
├── helper.h
├── source.bin
└── README.md
```

**Note:** The Phase 01 debugger is a dependency of the backend server and must be available in the expected relative directory.

## Requirements

- Linux or Windows Subsystem for Linux (WSL)
- GCC/G++ compiler supporting C++17
- POSIX socket support
- pthread library
- Phase 01 Time Traveller Debugger

## Installation

Clone the repository:

```bash
git clone https://github.com/alikashif4185-alt/Time-Traveller-Debugger-_Phase02.git
```

Navigate to the project directory:

```bash
cd Time-Traveller-Debugger-_Phase02
```

## Compilation

Compile the client:

```bash
g++ -std=c++17 -Wall -Wextra -pthread client.cpp -o client
```

Compile the Load Balancer:

```bash
g++ -std=c++17 -Wall -Wextra -pthread loadbalancer.cpp -o load_balancer
```

Compile the backend server:

```bash
g++ -std=c++17 -Wall -Wextra -pthread server.cpp -o server
```

The Phase 01 debugger must also be compiled separately in its directory:

```bash
g++ -std=c++17 -Wall -Wextra bscs25141_Dsa_TimeTravellerDebugger.cpp -o ttdb_linux
```

## Running the Application

### Step 1: Start the Load Balancer

From the Phase 02 project directory, execute:

```bash
./load_balancer 127.0.0.1 5001
```

The Load Balancer listens for client connections on port `5000` and forwards requests to the backend server on port `5001`.

### Step 2: Start the Client

Open another terminal in the Phase 02 directory:

```bash
./client 127.0.0.1 5000
```

### Step 3: Upload a Source File

At the client prompt, enter:

```text
ttdb> start source.bin
```

The client uploads the source file to the backend through the Load Balancer.

### Step 4: Navigate Through Execution History

After successful processing, use the navigation commands:

```text
next
prv
jmp
```

These commands allow navigation through the generated execution snapshots.

Use the `shutdown` command to exit the client.

## Communication Protocol

The client sends the source file followed by its CRC32 checksum.

The backend server:

1. Receives the uploaded source file.
2. Verifies its CRC32 checksum.
3. Saves the source file.
4. Executes the Phase 01 debugger.
5. Generates the `.tdbg` execution history.
6. Sends the result back to the client.

The response format is:

```text
[Status Byte][Payload][CRC32 Checksum]
```

| Status | Description |
|---|---|
| `0` | Successful execution |
| `1` | Error response |

## Future Improvements

- Support for multiple backend servers.
- Improved load distribution.
- Enhanced connection recovery.
- Additional debugging and navigation commands.

## Author

**Time Traveller Debugger — Phase 02**

Developed as part of an academic Data Structures and Algorithms project.
