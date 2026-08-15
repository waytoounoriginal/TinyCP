#ifndef TCP_FROM_SCRATCH_PLATFORM_H
#define TCP_FROM_SCRATCH_PLATFORM_H

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <basetsd.h>
#include <io.h>
#pragma comment(lib, "ws2_32.lib")

typedef SSIZE_T ssize_t;

#ifdef ERROR
#undef ERROR
#endif
#ifdef send
#undef send
#endif
#ifdef recv
#undef recv
#endif

#include <ctime>
inline struct tm* localtime_r(const time_t* timer, struct tm* buf) {
    localtime_s(buf, timer);
    return buf;
}

inline int read(int fd, void* buf, unsigned int count) { return _read(fd, buf, count); }
inline int write(int fd, const void* buf, unsigned int count) { return _write(fd, buf, count); }
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif

#endif // TCP_FROM_SCRATCH_PLATFORM_H
