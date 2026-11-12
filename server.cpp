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
