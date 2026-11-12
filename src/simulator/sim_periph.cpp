#include "simio.h"
#include "extern.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr int kMaxSerialPorts = 16;

struct SerialPortState {
    int fd = -1;
    bool initialized = false;
};

struct TcpClientConn {
    int fd = -1;
    uint32_t addrHostOrder = 0;
    std::string ipStr;
    uint16_t port = 0;
};

struct NetState {
    int mainSock = -1;
    bool initialized = false;
    bool isServer = false;
    bool isTcp = false;
    uint16_t localPort = 0;
    sockaddr_in remoteAddr{};
    std::vector<TcpClientConn> clients;
};

SerialPortState g_serialPorts[kMaxSerialPorts];
NetState g_net;

std::string resolveSerialDevicePath(const char *portName)
{
    if (!portName || !portName[0]) {
        return {};
    }
    std::string name(portName);
    // Map Windows-style "COMn" (e.g. COM1 -> /dev/ttyS0) while preserving Unix paths
    if (name.size() >= 4 &&
        (name[0] == 'C' || name[0] == 'c') &&
        (name[1] == 'O' || name[1] == 'o') &&
        (name[2] == 'M' || name[2] == 'm')) {
        char *endPtr = nullptr;
        long comNum = std::strtol(name.c_str() + 3, &endPtr, 10);
        if (endPtr != name.c_str() + 3 && *endPtr == '\0' && comNum >= 1 && comNum <= 64) {
            return "/dev/ttyS" + std::to_string(comNum - 1);
        }
    }
    return name;
}

speed_t mapBaudRate(int code)
{
    switch (code) {
    case 1:  return B110;
    case 2:  return B300;
    case 3:  return B600;
    case 4:  return B1200;
    case 5:  return B2400;
    case 6:  return B4800;
    case 0:
    case 7:  return B9600;
    case 8:  return B19200;
    case 9:  return B38400;
    case 10:
    case 11: return B57600;
    case 12:
    case 13: return B115200;
    case 14: return B230400;
    default: return B9600;
    }
}

bool setSocketNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void closeAllNetworkSockets()
{
    for (auto &c : g_net.clients) {
        if (c.fd >= 0) {
            ::close(c.fd);
        }
    }
    g_net.clients.clear();
    if (g_net.mainSock >= 0) {
        ::close(g_net.mainSock);
        g_net.mainSock = -1;
    }
    g_net.initialized = false;
    g_net.isServer = false;
    g_net.isTcp = false;
    g_net.localPort = 0;
    std::memset(&g_net.remoteAddr, 0, sizeof(g_net.remoteAddr));
}

void acceptPendingTcpClients()
{
    if (!g_net.initialized || !g_net.isServer || !g_net.isTcp || g_net.mainSock < 0) {
        return;
    }
    for (;;) {
        sockaddr_in cliAddr{};
        socklen_t cliLen = sizeof(cliAddr);
        int cliFd = ::accept(g_net.mainSock, reinterpret_cast<sockaddr*>(&cliAddr), &cliLen);
        if (cliFd < 0) {
            break;
        }
        setSocketNonBlocking(cliFd);
        char ipBuf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &cliAddr.sin_addr, ipBuf, sizeof(ipBuf));
        TcpClientConn conn;
        conn.fd = cliFd;
        conn.addrHostOrder = ntohl(cliAddr.sin_addr.s_addr);
        conn.ipStr = ipBuf;
        conn.port = ntohs(cliAddr.sin_port);
        g_net.clients.push_back(conn);
    }
}

void safeWriteMemString(char *dest, const std::string &src, size_t maxBytes)
{
    if (!dest || maxBytes == 0) return;
    // Clamp write strictly within simulator memory array if pointer lies inside it
    uintptr_t uDest = reinterpret_cast<uintptr_t>(dest);
    uintptr_t uMem = reinterpret_cast<uintptr_t>(memory);
    size_t avail = maxBytes;
    if (memory && uDest >= uMem && uDest < uMem + MEMSIZE) {
        size_t offset = static_cast<size_t>(uDest - uMem);
        avail = std::min(avail, static_cast<size_t>(MEMSIZE) - offset);
    }
    if (avail == 0) return;
    size_t copyLen = std::min(src.size(), avail - 1);
    if (copyLen > 0) {
        std::memcpy(dest, src.data(), copyLen);
    }
    dest[copyLen] = '\0';
}

} // namespace

// -----------------------------------------------------------------------------
// Serial I/O (TRAP #15 Tasks 40-43)
// -----------------------------------------------------------------------------
void SimIO::initComm(int cid, char *portName, short *result)
{
    auto setRes = [result](short v) {
        if (result) *result = v;
    };

    if (cid < 0 || cid >= kMaxSerialPorts) {
        setRes(1); // Invalid PID
        return;
    }

    if (g_serialPorts[cid].fd >= 0) {
        ::close(g_serialPorts[cid].fd);
        g_serialPorts[cid].fd = -1;
        g_serialPorts[cid].initialized = false;
    }

    std::string devPath = resolveSerialDevicePath(portName);
    if (devPath.empty()) {
        setRes(2); // Error
        return;
    }

    int fd = ::open(devPath.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        setRes(2);
        return;
    }

    // Apply default 9600 8N1 if device is a TTY; non-TTY files/FIFOs still open for testing
    struct termios tty{};
    if (tcgetattr(fd, &tty) == 0) {
        cfmakeraw(&tty);
        cfsetispeed(&tty, B9600);
        cfsetospeed(&tty, B9600);
        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
        tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 1;
        tcsetattr(fd, TCSANOW, &tty);
    }

    g_serialPorts[cid].fd = fd;
    g_serialPorts[cid].initialized = true;
    setRes(0);
}

void SimIO::setCommParams(int cid, int settings, short *result)
{
    auto setRes = [result](short v) {
        if (result) *result = v;
    };

    if (cid < 0 || cid >= kMaxSerialPorts) {
        setRes(1);
        return;
    }
    if (!g_serialPorts[cid].initialized || g_serialPorts[cid].fd < 0) {
        setRes(3); // Port not initialized
        return;
    }

    struct termios tty{};
    if (tcgetattr(g_serialPorts[cid].fd, &tty) != 0) {
        // Non-TTY stream (e.g. PTY/pipe test fixture): accept config gracefully
        setRes(0);
        return;
    }

    cfmakeraw(&tty);
    speed_t baud = mapBaudRate(settings & 0xFF);
    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);

    int dataBits = (settings >> 10) & 0x03;
    tty.c_cflag &= ~CSIZE;
    if (dataBits == 1) tty.c_cflag |= CS7;
    else if (dataBits == 2) tty.c_cflag |= CS6;
    else tty.c_cflag |= CS8;

    int parity = (settings >> 8) & 0x03;
    tty.c_cflag &= ~(PARENB | PARODD);
    if (parity == 1) {
        tty.c_cflag |= (PARENB | PARODD);
    } else if (parity == 2) {
        tty.c_cflag |= PARENB;
    }

    int stopBits = (settings >> 12) & 0x01;
    if (stopBits == 1) tty.c_cflag |= CSTOPB;
    else tty.c_cflag &= ~CSTOPB;

    tty.c_cflag |= (CLOCAL | CREAD);

    if (tcsetattr(g_serialPorts[cid].fd, TCSANOW, &tty) != 0) {
        setRes(2);
        return;
    }
    setRes(0);
}

void SimIO::readComm(int cid, unsigned char *n, char *str, short *result)
{
    auto setRes = [result](short v) {
        if (result) *result = v;
    };

    if (cid < 0 || cid >= kMaxSerialPorts) {
        setRes(1);
        return;
    }
    if (!g_serialPorts[cid].initialized || g_serialPorts[cid].fd < 0) {
        setRes(3);
        return;
    }
    if (!n || !str) {
        setRes(2);
        return;
    }

    unsigned char requested = *n;
    if (requested == 0) {
        str[0] = '\0';
        setRes(0);
        return;
    }

    // Clamp to simulator memory bounds if inside memory[]
    size_t maxBuf = requested;
    if (memory) {
        uintptr_t uStr = reinterpret_cast<uintptr_t>(str);
        uintptr_t uMem = reinterpret_cast<uintptr_t>(memory);
        if (uStr >= uMem && uStr < uMem + MEMSIZE) {
            size_t rem = static_cast<size_t>(MEMSIZE) - static_cast<size_t>(uStr - uMem);
            if (rem <= 1) {
                *n = 0;
                setRes(2);
                return;
            }
            maxBuf = std::min(maxBuf, rem - 1);
        }
    }

    size_t totalRead = 0;
    while (totalRead < maxBuf) {
        struct pollfd pfd{};
        pfd.fd = g_serialPorts[cid].fd;
        pfd.events = POLLIN;
        int pr = ::poll(&pfd, 1, 100);
        if (pr < 0) {
            if (errno == EINTR) continue;
            str[totalRead] = '\0';
            *n = static_cast<unsigned char>(totalRead);
            setRes(2);
            return;
        }
        if (pr == 0) {
            break; // Timeout
        }
        ssize_t r = ::read(g_serialPorts[cid].fd, str + totalRead, maxBuf - totalRead);
        if (r > 0) {
            totalRead += static_cast<size_t>(r);
        } else if (r == 0) {
            break;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            str[totalRead] = '\0';
            *n = static_cast<unsigned char>(totalRead);
            setRes(2);
            return;
        }
    }

    str[totalRead] = '\0';
    *n = static_cast<unsigned char>(totalRead);
    setRes(totalRead < maxBuf ? 4 : 0);
}

void SimIO::sendComm(int cid, unsigned char *n, char *str, short *result)
{
    auto setRes = [result](short v) {
        if (result) *result = v;
    };

    if (cid < 0 || cid >= kMaxSerialPorts) {
        setRes(1);
        return;
    }
    if (!g_serialPorts[cid].initialized || g_serialPorts[cid].fd < 0) {
        setRes(3);
        return;
    }
    if (!n || !str) {
        setRes(2);
        return;
    }

    unsigned char toSend = *n;
    size_t totalSent = 0;
    while (totalSent < toSend) {
        struct pollfd pfd{};
        pfd.fd = g_serialPorts[cid].fd;
        pfd.events = POLLOUT;
        int pr = ::poll(&pfd, 1, 100);
        if (pr < 0) {
            if (errno == EINTR) continue;
            *n = static_cast<unsigned char>(totalSent);
            setRes(2);
            return;
        }
        if (pr == 0) {
            break; // Timeout
        }
        ssize_t w = ::write(g_serialPorts[cid].fd, str + totalSent, toSend - totalSent);
        if (w > 0) {
            totalSent += static_cast<size_t>(w);
        } else if (w < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            *n = static_cast<unsigned char>(totalSent);
            setRes(2);
            return;
        }
    }

    *n = static_cast<unsigned char>(totalSent);
    setRes(totalSent < toSend ? 4 : 0);
}

// -----------------------------------------------------------------------------
// Network I/O (TRAP #15 Tasks 100-107)
// -----------------------------------------------------------------------------
void SimIO::createNetClient(int settings, char *server, int *result)
{
    closeAllNetworkSockets();

    int proto = settings & 0xFF;
    uint16_t port = static_cast<uint16_t>((static_cast<uint32_t>(settings) >> 16) & 0xFFFF);
    if ((proto != 0 && proto != 1) || !server || !server[0]) {
        if (result) *result = 1;
        return;
    }

    bool tcp = (proto == 1);
    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = tcp ? SOCK_STREAM : SOCK_DGRAM;

    struct addrinfo *res = nullptr;
    std::string portStr = std::to_string(port);
    int gai = getaddrinfo(server, portStr.c_str(), &hints, &res);
    if (gai != 0 || !res) {
        if (result) *result = (gai == EAI_NONAME) ? 8 : 4;
        return;
    }

    int sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        if (result) *result = 3;
        return;
    }

    sockaddr_in targetAddr{};
    std::memcpy(&targetAddr, res->ai_addr, sizeof(sockaddr_in));
    freeaddrinfo(res);

    if (tcp) {
        if (::connect(sock, reinterpret_cast<sockaddr*>(&targetAddr), sizeof(targetAddr)) < 0) {
            ::close(sock);
            if (result) *result = 6;
            return;
        }
    }

    if (!setSocketNonBlocking(sock)) {
        ::close(sock);
        if (result) *result = 2;
        return;
    }

    g_net.mainSock = sock;
    g_net.initialized = true;
    g_net.isServer = false;
    g_net.isTcp = tcp;
    g_net.localPort = port;
    g_net.remoteAddr = targetAddr;

    char ipBuf[INET_ADDRSTRLEN] = {};
    if (inet_ntop(AF_INET, &targetAddr.sin_addr, ipBuf, sizeof(ipBuf))) {
        safeWriteMemString(server, ipBuf, 64);
    }

    if (result) *result = 0;
}

void SimIO::createNetServer(int settings, int *result)
{
    closeAllNetworkSockets();

    int proto = settings & 0xFF;
    uint16_t port = static_cast<uint16_t>((static_cast<uint32_t>(settings) >> 16) & 0xFFFF);
    if (proto != 0 && proto != 1) {
        if (result) *result = 1;
        return;
    }

    bool tcp = (proto == 1);
    int sock = ::socket(AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (sock < 0) {
        if (result) *result = 3;
        return;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddr.sin_port = htons(port);

    if (::bind(sock, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) < 0) {
        int err = errno;
        ::close(sock);
        if (result) *result = (err == EADDRINUSE) ? 7 : 5;
        return;
    }

    if (tcp && ::listen(sock, 8) < 0) {
        ::close(sock);
        if (result) *result = 5;
        return;
    }

    if (!setSocketNonBlocking(sock)) {
        ::close(sock);
        if (result) *result = 2;
        return;
    }

    // Resolve actual bound port (supports ephemeral port 0 in tests)
    socklen_t addrLen = sizeof(bindAddr);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&bindAddr), &addrLen) == 0) {
        port = ntohs(bindAddr.sin_port);
    }

    g_net.mainSock = sock;
    g_net.initialized = true;
    g_net.isServer = true;
    g_net.isTcp = tcp;
    g_net.localPort = port;

    if (result) *result = 0;
}

void SimIO::closeNetConnection(int closeIP, int *result)
{
    if (!g_net.initialized) {
        closeAllNetworkSockets();
        if (result) *result = 0;
        return;
    }

    if (g_net.isServer && g_net.isTcp && closeIP != 0) {
        uint32_t targetHost = static_cast<uint32_t>(closeIP);
        for (auto it = g_net.clients.begin(); it != g_net.clients.end();) {
            if (it->addrHostOrder == targetHost) {
                ::close(it->fd);
                it = g_net.clients.erase(it);
            } else {
                ++it;
            }
        }
        if (result) *result = 0;
        return;
    }

    closeAllNetworkSockets();
    if (result) *result = 0;
}

void SimIO::getLocalIP(char *localIP, int *result)
{
    if (!localIP) {
        if (result) *result = 1;
        return;
    }

    std::string chosen = "127.0.0.1";
    struct ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) == 0 && ifap) {
        for (struct ifaddrs *ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            auto *sa = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            char buf[INET_ADDRSTRLEN] = {};
            if (inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf))) {
                std::string candidate(buf);
                if (candidate != "127.0.0.1") {
                    chosen = candidate;
                    break;
                }
            }
        }
        freeifaddrs(ifap);
    }

    safeWriteMemString(localIP, chosen, 16);
    if (result) *result = 0;
}

void SimIO::sendNet(int settings, char *data, char *remoteIP, int *count, int *result)
{
    long d0 = 0;
    long d1 = static_cast<long>(static_cast<uint32_t>(settings) & 0xFFFFU) |
              (static_cast<long>(g_net.localPort) << 16);
    sendPortNet(&d0, &d1, data, remoteIP);
    if (result) *result = static_cast<int>(d0);
    if (count && d0 == 0) *count = static_cast<int>(d1 & 0xFFFF);
}

void SimIO::receiveNet(int settings, char *buffer, int *count, char *senderIP, int *result)
{
    long d0 = 0;
    long d1 = static_cast<long>(static_cast<uint32_t>(settings) & 0xFFFFU);
    receivePortNet(&d0, &d1, buffer, senderIP);
    if (result) *result = static_cast<int>(d0);
    if (count && d0 == 0) *count = static_cast<int>(d1 & 0xFFFF);
}

void SimIO::sendPortNet(long *D0, long *D1, char *data, char *remoteIP)
{
    auto fail = [D0]() {
        if (D0) *D0 = 1;
    };

    if (!g_net.initialized || g_net.mainSock < 0 || !data || !D1) {
        fail();
        return;
    }

    uint16_t len = static_cast<uint16_t>(*D1 & 0xFFFF);
    uint16_t port = static_cast<uint16_t>((static_cast<uint32_t>(*D1) >> 16) & 0xFFFF);
    if (port == 0) {
        port = g_net.localPort;
    }

    ssize_t sent = 0;
    if (g_net.isTcp) {
        if (g_net.isServer) {
            acceptPendingTcpClients();
            int targetFd = -1;
            if (remoteIP && remoteIP[0]) {
                for (const auto &c : g_net.clients) {
                    if (c.ipStr == remoteIP) {
                        targetFd = c.fd;
                        break;
                    }
                }
            }
            if (targetFd < 0 && !g_net.clients.empty()) {
                targetFd = g_net.clients.front().fd;
            }
            if (targetFd < 0) {
                fail();
                return;
            }
            sent = ::send(targetFd, data, len, MSG_NOSIGNAL);
        } else {
            sent = ::send(g_net.mainSock, data, len, MSG_NOSIGNAL);
        }
    } else {
        sockaddr_in dst{};
        dst.sin_family = AF_INET;
        dst.sin_port = htons(port);
        if (g_net.isServer) {
            if (!remoteIP || !remoteIP[0] || inet_pton(AF_INET, remoteIP, &dst.sin_addr) != 1) {
                fail();
                return;
            }
        } else {
            if (remoteIP && remoteIP[0] && inet_pton(AF_INET, remoteIP, &dst.sin_addr) == 1) {
                // Use explicit remoteIP if valid
            } else {
                dst.sin_addr = g_net.remoteAddr.sin_addr;
            }
        }
        sent = ::sendto(g_net.mainSock, data, len, 0,
                        reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
    }

    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (D0) *D0 = 0;
            *D1 = (*D1 & ~0xFFFFL);
            return;
        }
        fail();
        return;
    }

    if (D0) *D0 = 0;
    *D1 = (*D1 & ~0xFFFFL) | static_cast<long>(sent & 0xFFFF);
}

void SimIO::receivePortNet(long *D0, long *D1, char *buffer, char *senderIP)
{
    auto fail = [D0]() {
        if (D0) *D0 = 1;
    };

    if (!g_net.initialized || g_net.mainSock < 0 || !buffer || !D1) {
        fail();
        return;
    }

    uint16_t maxLen = static_cast<uint16_t>(*D1 & 0xFFFF);
    if (memory) {
        uintptr_t uBuf = reinterpret_cast<uintptr_t>(buffer);
        uintptr_t uMem = reinterpret_cast<uintptr_t>(memory);
        if (uBuf >= uMem && uBuf < uMem + MEMSIZE) {
            size_t avail = static_cast<size_t>(MEMSIZE) - static_cast<size_t>(uBuf - uMem);
            if (maxLen > avail) maxLen = static_cast<uint16_t>(avail);
        }
    }

    ssize_t recvd = -1;
    uint16_t srcPort = g_net.localPort;
    std::string srcIp;

    if (g_net.isTcp) {
        if (g_net.isServer) {
            acceptPendingTcpClients();
            for (auto it = g_net.clients.begin(); it != g_net.clients.end();) {
                ssize_t r = ::recv(it->fd, buffer, maxLen, 0);
                if (r > 0) {
                    recvd = r;
                    srcIp = it->ipStr;
                    srcPort = it->port;
                    break;
                } else if (r == 0) {
                    ::close(it->fd);
                    it = g_net.clients.erase(it);
                    continue;
                } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    ::close(it->fd);
                    it = g_net.clients.erase(it);
                    continue;
                }
                ++it;
            }
            if (recvd < 0) {
                // Non-blocking: 0 bytes ready
                if (D0) *D0 = 0;
                *D1 = 0;
                return;
            }
        } else {
            recvd = ::recv(g_net.mainSock, buffer, maxLen, 0);
            if (recvd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                if (D0) *D0 = 0;
                *D1 = 0;
                return;
            }
            if (recvd <= 0) {
                fail();
                return;
            }
            char ipBuf[INET_ADDRSTRLEN] = {};
            if (inet_ntop(AF_INET, &g_net.remoteAddr.sin_addr, ipBuf, sizeof(ipBuf))) {
                srcIp = ipBuf;
            }
            srcPort = ntohs(g_net.remoteAddr.sin_port);
        }
    } else {
        sockaddr_in srcAddr{};
        socklen_t srcLen = sizeof(srcAddr);
        recvd = ::recvfrom(g_net.mainSock, buffer, maxLen, 0,
                           reinterpret_cast<sockaddr*>(&srcAddr), &srcLen);
        if (recvd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (D0) *D0 = 0;
                *D1 = 0;
                return;
            }
            fail();
            return;
        }
        char ipBuf[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &srcAddr.sin_addr, ipBuf, sizeof(ipBuf))) {
            srcIp = ipBuf;
        }
        srcPort = ntohs(srcAddr.sin_port);
    }

    if (senderIP && !srcIp.empty()) {
        safeWriteMemString(senderIP, srcIp, 64);
    }

    if (D0) *D0 = 0;
    *D1 = (static_cast<long>(srcPort) << 16) | static_cast<long>(recvd & 0xFFFF);
}
