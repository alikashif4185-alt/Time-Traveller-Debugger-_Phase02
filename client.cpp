// ======================= TIME-TRAVEL DEBUGGER - CLIENT TEMPLATE =======================

// The client never talks to the server directly. It only talks to the load balancer.
// The load balancer wakes the server up if it is down, so the client does not care.
// The first "start" may just take a bit longer.
//
// The network is used ONLY by "start". After the .tdbg file is downloaded,
// next / prv / jmp read that file on the local disk. They never contact the load balancer.
//
// How the pieces fit together:
//   main loop -> readCommand -> runCommand
//   start -> cmdStart()  -> requestDebugFile() (Part A) -> openTdbg() (Part B) -> shows step 1
//   next, prv, jmp -> moveBy() (Part D) -> readSnapshotAt() (Part B) -> printSnapshot() (Part E)

#include <iostream>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <vector>
#include <sys/socket.h>
#include "helper.h"
#include<sstream>
#include<cerrno>
using namespace std;

// ---- Constants ----
const char* PROGRAM_NAME = "ttdb";
const char* PROMPT = "ttdb> ";
const int IO_BUFFER_SIZE = 64 * 1024;     // fixed buffer for streaming files to and from the socket
const char* TDBG_FILE = "ttdb_last.tdbg"; // where the downloaded .tdbg is saved
const int MAX_JUMP = 1000000000;          // biggest n accepted by jmp
string LB_HOST = "127.0.0.0";
int LB_PORT = 5000;

// These two must be the same as in Phase 1

const int MAX_VARS_PER_FRAME = 16;
const int MAX_STACK_DEPTH = 64;


// ---- Structs (same as Phase 1, so the file can be read back exactly as it was written) ----
struct Variable
{
    string name;
    int32_t value;
};
struct Frame
{
    string func_name;
    int32_t argc;
    Variable argv[MAX_VARS_PER_FRAME];
    int32_t returnLine;
    Variable locals[MAX_VARS_PER_FRAME];
    int32_t localCount;
};
struct Snapshot
{
    Frame callStack[MAX_STACK_DEPTH];
    int32_t stackDepth;
};
struct TTDBHeader
{
    char magic[4]; // "TTDB"
    int32_t version;
    int32_t stepCount;
    int64_t indexOffset;
};

// ---- Structs only the client needs ----
struct TdbgReader
{
    ssize_t file_index;
    TTDBHeader header;
    vector<int64_t> index;
};
struct DebugSession
{
    int active_flag;     // false until a "start" has worked
    TdbgReader reader;   // the .tdbg file stays open for the whole session
    int64_t currentStep;
};

// =============================================================================================
// PART A: TALKING TO THE LOAD BALANCER
// =============================================================================================

bool recvAll(int sock, char* buf, size_t n)
{
    size_t t = 0;
    while (t <= n) {
        ssize_t bytes = recv(sock, buf + t, n - t, 0);
        if (bytes <= 0) {
            return false;
        }

        t += bytes;
    }
    return true;
    // recv() can return fewer bytes than asked for, so keep calling it until exactly n bytes are read.
    // Return false if the connection closes or fails before n bytes arrive.
}
bool sendAll(int sock, const char* buf, size_t n)
{
    size_t t = 0;
    while (t <= n) {
        ssize_t sendbytes = send(sock, t + buf, t - n, MSG_NOSIGNAL);
        if (sendbytes <= 0) {
            return false;
        }
        t += sendbytes;
    }

    return true;
    // send() can send fewer bytes than asked for, so keep calling it until all n bytes are sent.
    // Use the flag MSG_NOSIGNAL. Return false if the connection fails.
}

int connectToHost(const char* host, int port)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        return -1;
    }
    struct addrinfo hints {}, * result;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[10];
    snprintf(portStr, sizeof(portStr), "%d", port);

    if (getaddrinfo(host, portStr, &hints, &result) != 0)
    {
        close(socket_fd);
        return -1;
    }
    if (connect(socket_fd, result->ai_addr, result->ai_addrlen) == -1)
    {
        freeaddrinfo(result);
        close(socket_fd);
        return -1;
    }
    freeaddrinfo(result);

    return s;
    // Create a TCP socket and connect it to host:port.
    // Return the socket, or -1 if the connection fails.
}

uint32_t crc32Update(uint32_t crc, const unsigned char* data, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        crc ^= data[i];

        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }

    return crc;
}
bool uploadSource(int sock, const char* sourcePath) {
    FILE* f = fopen(sourcePath, "rb");

    if (f == nullptr)
        return false;

    char buf[4096];
    uint32_t crc = 0xFFFFFFFF;

    while (true)
    {
        size_t n = fread(buf, 1, sizeof(buf), f);

        if (n > 0) {
            crc = crc32Update(crc, (unsigned char*)buf, n);

            if (!sendAll(sock, buf, n)) {
                fclose(f);
                return false;
            }
        }
        if (n < sizeof(buf)) {
            if (ferror(f)) {
                fclose(f);
                return false;
            }
            break;
        }
    }
    fclose(f);

    crc ^= 0xFFFFFFFF;
    uint32_t checksum = htonl(crc);
    if (!sendAll(sock, (char*)&checksum, sizeof(checksum)))
        return false;

    if (shutdown(sock, SHUT_WR) < 0)
        return false;

    return true;
    // When the file is finished, call shutdown(sock, SHUT_WR). This is how the server knows the string is complete.
    // Return false if the file cannot be opened or any send fails.
}
bool receiveResponse(int sock, const char* outTdbgPath, string& outError) {
    TrailerChecker checker;

    char buf[4096];

    while (true) {
        ssize_t n = recv(sock, buf, sizeof(buf), 0);

        if (n == 0)
            break;

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        checker.feed(buf, n);
