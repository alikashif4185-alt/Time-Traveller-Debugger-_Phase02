#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <iterator>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace std;

uint32_t crc32Update(uint32_t crc, const unsigned char* data, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        crc ^= data[i];

        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }

    return crc;
}

bool sendAll(int sock, const char* buf, size_t n)
{
    size_t t = 0;

    while (t < n) {
        ssize_t b = send(sock, buf + t, n - t, MSG_NOSIGNAL);

        if (b <= 0)
            return false;

        t += b;
    }

    return true;
}

bool sendResponse(int sock, unsigned char status,
    const vector<unsigned char>& payload)
{
    uint32_t crc = 0xFFFFFFFF;

    crc = crc32Update(crc, &status, 1);

    if (!payload.empty())
        crc = crc32Update(crc, payload.data(), payload.size());

    crc ^= 0xFFFFFFFF;

    uint32_t checksum = htonl(crc);

    if (!sendAll(sock, (char*)&status, 1))
        return false;

    if (!payload.empty()) {
        if (!sendAll(sock, (char*)payload.data(), payload.size()))
            return false;
    }

    return sendAll(sock, (char*)&checksum, sizeof(checksum));
}

vector<unsigned char> makePayload(const string& message)
{
    return vector<unsigned char>(message.begin(), message.end());
}

void processClient(int clientSock)
{
    vector<unsigned char> source;

    unsigned char trailer[4];
    size_t trailerCount = 0;

    char buf[4096];

    // Receive source file and its final 4-byte checksum.
    while (true) {
        ssize_t n = recv(clientSock, buf, sizeof(buf), 0);

        if (n == 0)
            break;

        if (n < 0)
            return;

        for (ssize_t i = 0; i < n; i++) {
            if (trailerCount < 4) {
                trailer[trailerCount++] =
                    (unsigned char)buf[i];
            }
            else {
                source.push_back(trailer[0]);

                for (int j = 0; j < 3; j++)
                    trailer[j] = trailer[j + 1];

                trailer[3] = (unsigned char)buf[i];
            }
        }
    }

    if (trailerCount != 4 || source.empty()) {
        sendResponse(clientSock, 1,
            makePayload("Invalid or empty upload"));
        return;
    }

    // Verify uploaded source checksum.
    uint32_t crc = crc32Update(
        0xFFFFFFFF, source.data(), source.size());

    crc ^= 0xFFFFFFFF;

    uint32_t received;
    memcpy(&received, trailer, 4);

    if (crc != ntohl(received)) {
        sendResponse(clientSock, 1,
            makePayload("Source checksum mismatch"));
        return;
    }

    // Save uploaded source in Phase 1 directory.
    ofstream out("source.bin", ios::binary);

    if (!out.is_open()) {
        sendResponse(clientSock, 1,
            makePayload("Cannot create source.bin"));
        return;
    }

    out.write((char*)source.data(), source.size());
    out.close();

    if (!out) {
        sendResponse(clientSock, 1,
            makePayload("Could not save source.bin"));
        return;
    }

    // Remove previous output so stale files cannot be returned.
    remove("session.tdbg");
    remove("resolve.bin");
    remove("backend_error.txt");

    // Run the existing Phase 1 debugger.
    int result = system("./ttdb_linux > backend_error.txt 2>&1");

    if (result != 0) {
        ifstream err("backend_error.txt");
        string message(
            (istreambuf_iterator<char>(err)),
            istreambuf_iterator<char>());

        if (message.empty())
            message = "Phase 1 debugger failed";

        sendResponse(clientSock, 1, makePayload(message));
        return;
    }

    // Read generated debugging file.
    ifstream file("session.tdbg", ios::binary);

    if (!file.is_open()) {
        sendResponse(clientSock, 1,
            makePayload("session.tdbg was not generated"));
        return;
    }

    vector<unsigned char> debugData(
        (istreambuf_iterator<char>(file)),
        istreambuf_iterator<char>());

    if (debugData.empty()) {
        sendResponse(clientSock, 1,
            makePayload("Generated session.tdbg is empty"));
        return;
    }

    // Status 0 means success.
    sendResponse(clientSock, 0, debugData);
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        cerr << "Usage: ./server <port>\n";
        return 1;
    }

    int port = atoi(argv[1]);

    // Phase 1 files and executable are in this directory.
    if (chdir("../Phase 01") != 0) {
        perror("Cannot enter Phase 01 directory");
        return 1;
    }

    int listenSock = socket(AF_INET, SOCK_STREAM, 0);

    if (listenSock < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
