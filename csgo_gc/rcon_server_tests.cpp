#include "stdafx.h"
#include "gc_client.h"
#include "rcon_server.h"
#include "test_filesystem.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace
{

std::mutex s_logMutex;
std::condition_variable s_logChanged;
std::vector<std::string> s_logMessages;

#ifdef _WIN32
using TestSocket = SOCKET;
constexpr TestSocket InvalidTestSocket = INVALID_SOCKET;
#else
using TestSocket = int;
constexpr TestSocket InvalidTestSocket = -1;
#endif

void CloseTestSocket(TestSocket socket)
{
    if (socket == InvalidTestSocket)
    {
        return;
    }

#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

void SleepMilliseconds(int milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds{ milliseconds });
}

bool WriteConfig(const char *bindAddress, uint16_t port, const char *password)
{
    if (!TestFilesystem::MakeDirectory("csgo_gc"))
    {
        return false;
    }

    std::string config;
    config += "\"rcon\"\n";
    config += "{\n";
    config += "    \"enabled\" \"1\"\n";
    config += "    \"bind_address\" \"";
    config += bindAddress;
    config += "\"\n";
    config += "    \"port\" \"";
    config += std::to_string(port);
    config += "\"\n";
    config += "    \"password\" \"";
    config += password;
    config += "\"\n";
    config += "}\n";

    FILE *file = fopen("csgo_gc/config.txt", "wb");
    if (!file)
    {
        return false;
    }

    bool success = fwrite(config.data(), 1, config.size(), file) == config.size();
    success &= fclose(file) == 0;
    return success;
}

bool WaitForLog(std::string_view fragment)
{
    std::unique_lock lock{ s_logMutex };
    return s_logChanged.wait_for(lock, std::chrono::seconds{ 5 }, [fragment] {
        return std::any_of(s_logMessages.begin(), s_logMessages.end(), [fragment](const std::string &message) {
            return message.find(fragment) != std::string::npos;
        });
    });
}

size_t CountLogs(std::string_view fragment)
{
    std::lock_guard lock{ s_logMutex };
    return static_cast<size_t>(std::count_if(s_logMessages.begin(), s_logMessages.end(), [fragment](const std::string &message) {
        return message.find(fragment) != std::string::npos;
    }));
}

uint16_t AllocateLoopbackPort()
{
    TestSocket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == InvalidTestSocket)
    {
        return 0;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (bind(socket, reinterpret_cast<const sockaddr *>(&address), static_cast<int>(sizeof(address))) != 0)
    {
        CloseTestSocket(socket);
        return 0;
    }

    sockaddr_in bound{};
#ifdef _WIN32
    int boundSize = sizeof(bound);
#else
    socklen_t boundSize = sizeof(bound);
#endif
    if (getsockname(socket, reinterpret_cast<sockaddr *>(&bound), &boundSize) != 0)
    {
        CloseTestSocket(socket);
        return 0;
    }

    CloseTestSocket(socket);
    return ntohs(bound.sin_port);
}

bool SendAll(TestSocket socket, const char *data, size_t size)
{
    while (size)
    {
        int sent = send(socket, data, static_cast<int>(size), 0);
        if (sent <= 0)
        {
            return false;
        }

        data += sent;
        size -= static_cast<size_t>(sent);
    }

    return true;
}

void AppendInt32LE(std::string &buffer, int32_t value)
{
    uint32_t unsignedValue = static_cast<uint32_t>(value);
    buffer.push_back(static_cast<char>(unsignedValue & 0xff));
    buffer.push_back(static_cast<char>((unsignedValue >> 8) & 0xff));
    buffer.push_back(static_cast<char>((unsignedValue >> 16) & 0xff));
    buffer.push_back(static_cast<char>((unsignedValue >> 24) & 0xff));
}

int32_t ReadInt32LE(const char *data)
{
    const auto *bytes = reinterpret_cast<const unsigned char *>(data);
    return static_cast<int32_t>(
        static_cast<uint32_t>(bytes[0])
        | (static_cast<uint32_t>(bytes[1]) << 8)
        | (static_cast<uint32_t>(bytes[2]) << 16)
        | (static_cast<uint32_t>(bytes[3]) << 24));
}

bool SendPacket(TestSocket socket, int32_t requestId, int32_t type, std::string_view body)
{
    std::string packet;
    packet.reserve(sizeof(int32_t) * 3 + body.size() + 2);
    AppendInt32LE(packet, static_cast<int32_t>(sizeof(int32_t) * 2 + body.size() + 2));
    AppendInt32LE(packet, requestId);
    AppendInt32LE(packet, type);
    packet.append(body.data(), body.size());
    packet.push_back('\0');
    packet.push_back('\0');
    return SendAll(socket, packet.data(), packet.size());
}

bool RecvAll(TestSocket socket, char *data, size_t size)
{
    while (size)
    {
        int received = recv(socket, data, static_cast<int>(size), 0);
        if (received <= 0)
        {
            return false;
        }

        data += received;
        size -= static_cast<size_t>(received);
    }

    return true;
}

bool RecvPacket(TestSocket socket, int32_t &requestId, int32_t &type, std::string &body)
{
    char sizeBytes[sizeof(int32_t)];
    if (!RecvAll(socket, sizeBytes, sizeof(sizeBytes)))
    {
        return false;
    }

    int32_t packetSize = ReadInt32LE(sizeBytes);
    if (packetSize < 10 || packetSize > 4096)
    {
        return false;
    }

    std::string packet;
    packet.resize(static_cast<size_t>(packetSize));
    if (!RecvAll(socket, packet.data(), packet.size()))
    {
        return false;
    }

    requestId = ReadInt32LE(packet.data());
    type = ReadInt32LE(packet.data() + sizeof(int32_t));

    size_t bodyOffset = sizeof(int32_t) * 2;
    size_t bodySize = packet.size() - bodyOffset;
    if (bodySize >= 2)
    {
        bodySize -= 2;
    }
    body.assign(packet.data() + bodyOffset, bodySize);
    return true;
}

bool ConnectToLoopback(uint16_t port, TestSocket &outSocket)
{
    for (int attempt = 0; attempt < 50; attempt++)
    {
        TestSocket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket == InvalidTestSocket)
        {
            return false;
        }

#ifdef _WIN32
        DWORD timeout = 3000;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout),
            static_cast<int>(sizeof(timeout)));
#else
        timeval timeout{};
        timeout.tv_sec = 3;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);

        if (connect(socket, reinterpret_cast<const sockaddr *>(&address), static_cast<int>(sizeof(address))) == 0)
        {
            outSocket = socket;
            return true;
        }

        CloseTestSocket(socket);
        SleepMilliseconds(100);
    }

    return false;
}

bool ListenerFailureDisablesRconWithoutRetrying()
{
    if (!WriteConfig("192.0.2.1", 37016, "test-password"))
    {
        return false;
    }

    RconServer server;
    server.Start();
    if (!WaitForLog("RCON: bind failed on 192.0.2.1:37016"))
    {
        return false;
    }

    server.RegisterClient(nullptr);
    server.Stop();

    return CountLogs("RCON: starting listener thread") == 1
        && CountLogs("RCON: disabled for this process after listener failure") == 1;
}

bool LoopbackAuthAndCommands()
{
    uint16_t port = AllocateLoopbackPort();
    if (!port)
    {
        return false;
    }

    if (!WriteConfig("127.0.0.1", port, "test-password"))
    {
        return false;
    }

    RconServer server;
    server.Start();

    std::string listening = "RCON: listening on 127.0.0.1:";
    listening += std::to_string(port);
    if (!WaitForLog(listening))
    {
        server.Stop();
        return false;
    }

    TestSocket socket = InvalidTestSocket;
    if (!ConnectToLoopback(port, socket))
    {
        server.Stop();
        return false;
    }

    bool success = true;

    // Wrong password: an empty response followed by an auth response with id -1.
    int32_t requestId = 0;
    int32_t type = 0;
    std::string body;
    success &= SendPacket(socket, 10, 3, "wrong-password");
    success &= RecvPacket(socket, requestId, type, body) && requestId == 10 && type == 0 && body.empty();
    success &= RecvPacket(socket, requestId, type, body) && requestId == -1 && type == 2 && body.empty();

    // Correct password.
    success &= SendPacket(socket, 11, 3, "test-password");
    success &= RecvPacket(socket, requestId, type, body) && requestId == 11 && type == 0 && body.empty();
    success &= RecvPacket(socket, requestId, type, body) && requestId == 11 && type == 2 && body.empty();

    // ping is answered before the GC is consulted.
    success &= SendPacket(socket, 12, 2, "ping");
    success &= RecvPacket(socket, requestId, type, body) && requestId == 12 && body == "OK pong";

    // No client registered in this test.
    success &= SendPacket(socket, 13, 2, "status");
    success &= RecvPacket(socket, requestId, type, body) && requestId == 13 && body == "OK rcon=enabled client=none";

    // help falls through to the static usage list when no client is registered.
    success &= SendPacket(socket, 14, 2, "help");
    success &= RecvPacket(socket, requestId, type, body) && requestId == 14 && body.rfind("OK commands:", 0) == 0;

    CloseTestSocket(socket);
    server.Stop();
    return success;
}

bool RunLoopbackTest()
{
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
    {
        return false;
    }
#endif

    bool success = LoopbackAuthAndCommands();

#ifdef _WIN32
    WSACleanup();
#endif
    return success;
}

} // namespace

namespace Platform
{

void Print(const char *format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    {
        std::lock_guard lock{ s_logMutex };
        s_logMessages.emplace_back(buffer);
    }
    s_logChanged.notify_all();
}

} // namespace Platform

std::string ClientGC::RunRconCommand(std::string)
{
    return {};
}

std::string ClientGC::RconCommandUsageList()
{
    return {};
}

int main(int argc, char **argv)
{
    const bool loopback = argc > 1 && std::strcmp(argv[1], "--loopback") == 0;
    const bool success = loopback ? RunLoopbackTest() : ListenerFailureDisablesRconWithoutRetrying();

    TestFilesystem::RemoveFile("csgo_gc/config.txt");
    TestFilesystem::RemoveDir("csgo_gc");
    return success ? 0 : 1;
}
