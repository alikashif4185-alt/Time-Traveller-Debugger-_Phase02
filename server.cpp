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
