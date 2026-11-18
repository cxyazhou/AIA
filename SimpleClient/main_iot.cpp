#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cctype>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSE_SOCKET closesocket
#define SOCKET_ERROR_MSG SOCKET_ERROR
using socket_t = SOCKET;
const int INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#define CLOSE_SOCKET close
#define SOCKET_ERROR_MSG (-1)
using socket_t = int;
const int INVALID_SOCKET_VALUE = -1;
#endif

// 获取当前 socket 错误码及描述（跨平台，用于日志输出）
inline std::string lastSocketError() {
#if defined(_WIN32)
    int code = WSAGetLastError();
    std::string desc;
    switch (code) {
        case 0:               desc = "无错误"; break;
        case 10004:           desc = "WSAEINTR(操作中断)"; break;
        case 10013:           desc = "WSAEACCES(权限拒绝/端口被系统保留)"; break;
        case 10022:           desc = "WSAEINVAL(参数无效)"; break;
        case 10035:           desc = "WSAEWOULDBLOCK(非阻塞操作无法立刻完成)"; break;
        case 10038:           desc = "WSAENOTSOCK(非socket描述符)"; break;
        case 10047:           desc = "WSAEAFNOSUPPORT(地址族不支持)"; break;
        case 10048:           desc = "WSAEADDRINUSE(端口被占用)"; break;
        case 10049:           desc = "WSAEADDRNOTAVAIL(地址不可用)"; break;
        case 10053:           desc = "WSAECONNABORTED(软件导致连接中止)"; break;
        case 10054:           desc = "WSAECONNRESET(远程主机强迫关闭连接)"; break;
        case 10057:           desc = "WSAENOTCONN(套接字未连接)"; break;
        case 10058:           desc = "WSAESHUTDOWN(套接字已关闭无法收发)"; break;
        case 10060:           desc = "WSAETIMEDOUT(连接超时)"; break;
        case 10061:           desc = "WSAECONNREFUSED(目标端口未监听/拒绝连接)"; break;
        case 10093:           desc = "WSANOTINITIALISED(WSAStartup未调用)"; break;
        default:              desc = "其他"; break;
    }
    return "code=" + std::to_string(code) + "(" + desc + ")";
#else
    int code = errno;
    return "errno=" + std::to_string(code) + "(" + std::string(strerror(code)) + ")";
#endif
}

#include "nlohmann/json.hpp"
using json = nlohmann::json;

// 消息类型定义
struct MessageType {
    static constexpr const char* REGISTER = "register";
    static constexpr const char* GET_CLIENTS = "get_clients";
    static constexpr const char* SEND_MESSAGE = "send_message";
    static constexpr const char* BROADCAST = "broadcast";
    static constexpr const char* PRIVATE_MESSAGE = "private";
    static constexpr const char* HEARTBEAT = "heartbeat";
    static constexpr const char* RESPONSE = "response";
};

// 消息类
class Message {
public:
    std::string type;
    json data;
    std::string sender;
    std::string target;

    Message();
    Message(const std::string& t, const json& d,
        const std::string& s = "", const std::string& tg = "");

    std::string toJson() const;
    static Message fromJson(const std::string& jsonStr);
    bool isValid() const { return !type.empty(); }
};

// TCP 客户端类
class TCPClient {
public:
    TCPClient(const std::string& host = "127.0.0.1", int port = 8180);
    ~TCPClient();

    bool connect(const std::string& name);
    void getClients();
    bool sendPrivateMessage(const std::string& target, const std::string& content);
    bool broadcast(const std::string& content);
    void closeConnect();
    bool isRunning() const { return m_running.load(); }
    bool isRegistered() const { return m_registered.load(); }

private:
    bool register_();
    bool sendMessage(const Message& msg);
    void receiveMessages();
    void handleMessage(const Message& msg);
    void heartbeatLoop();

private:
    std::string m_host;
    int m_port;
    socket_t m_socket;
    std::string m_name;
    std::atomic<bool> m_running;
    std::atomic<bool> m_registered;
    std::thread m_recvThread;
    std::thread m_heartbeatThread;
    std::mutex m_mutex;            // 保护 m_clientsList 等共享数据
    std::mutex m_sendMutex;        // 保护 sendMessage：心跳线程 + 其他调用者并发发消息
    std::vector<std::string> m_clientsList;
};

// 辅助函数
std::pair<std::string, int> parseServerAddress(const std::string& addrStr);
void printHelp();
bool testConnection(const std::string& host, int port);

// 跨平台获取 sockaddr（getaddrinfo 替代 gethostbyname）
bool resolveHost(const std::string& host, int port, sockaddr_in& out);

TCPClient* TcpClientInstance(const char* address, const char* name);
void TcpClientCloseInstance();

void TcpClientSendMsg(const char* name, const char* msg);

#if defined(_WIN32)
#include <windows.h>
// UTF-8 转 GBK，解决 Windows 控制台中文乱码
static std::string utf8ToGbk(const std::string& utf8) {
    if (utf8.empty()) return utf8;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    if (wlen <= 0) return utf8;
    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wstr[0], wlen);
    int gbkLen = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wlen, NULL, 0, NULL, NULL);
    if (gbkLen <= 0) return utf8;
    std::string gbk(gbkLen, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wlen, &gbk[0], gbkLen, NULL, NULL);
    return gbk;
}
#else
static const std::string& utf8ToGbk(const std::string& s) { return s; }
#endif

static void handlePrivateMessage(const std::string& sender, const std::string& content) {
    // 收到 name 为 ubuntu 的"重启"消息时，执行重启命令
    if (sender == "ubuntu" && content == "重启") {
        std::cout << utf8ToGbk("收到 ubuntu 重启指令，执行 reboot") << std::endl;
        TcpClientSendMsg(NULL, "开始重启");
        int rc = std::system("reboot");
        (void)rc;
        return;
    }
}

// Message 类实现
Message::Message() {}

Message::Message(const std::string& t, const json& d,
    const std::string& s, const std::string& tg)
    : type(t), data(d), sender(s), target(tg) {
}

std::string Message::toJson() const {
    json j;
    j["type"] = type;
    j["data"] = data;
    j["sender"] = sender;
    j["target"] = target;
    return j.dump();
}

Message Message::fromJson(const std::string& jsonStr) {
    try {
        auto j = json::parse(jsonStr);
        Message msg;
        msg.type = j.value("type", "");
        msg.data = j.value("data", json::object());
        msg.sender = j.value("sender", "");
        msg.target = j.value("target", "");
        return msg;
    }
    catch (const std::exception& e) {
        std::cerr << utf8ToGbk("JSON 解析失败: ") << e.what()
                  << " input=" << utf8ToGbk(jsonStr.substr(0, 100)) << std::endl;
        return Message();
    }
    catch (...) {
        std::cerr << utf8ToGbk("JSON 解析未知异常 input=")
                  << utf8ToGbk(jsonStr.substr(0, 100)) << std::endl;
        return Message();
    }
}

// 跨平台主机解析：优先 inet_pton，失败用 getaddrinfo（线程安全，替代 gethostbyname）
bool resolveHost(const std::string& host, int port, sockaddr_in& out) {
    std::memset(&out, 0, sizeof(out));
    out.sin_family = AF_INET;
    out.sin_port = htons(static_cast<uint16_t>(port));

    // 优先按点分十进制 IPv4 解析
    if (inet_pton(AF_INET, host.c_str(), &out.sin_addr) == 1) {
        return true;
    }

    // getaddrinfo 线程安全，支持 IPv6/IPv4
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    int err = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (err != 0 || res == nullptr) {
        if (res) freeaddrinfo(res);
#if defined(_WIN32)
        // gai_strerror 在部分 Windows 版本不可靠，打印 WSA 错误码
        std::cerr << "getaddrinfo('" << host << "') 失败: code=" << WSAGetLastError()
                  << " gai_code=" << err << " desc=" << gai_strerrorA(err) << std::endl;
#else
        std::cerr << "getaddrinfo('" << host << "') 失败: " << gai_strerror(err) << std::endl;
#endif
        return false;
    }
    std::memcpy(&out.sin_addr, &((struct sockaddr_in*)res->ai_addr)->sin_addr, sizeof(in_addr));
    freeaddrinfo(res);
    return true;
}

// TCPClient 类实现
TCPClient::TCPClient(const std::string& host, int port)
    : m_host(host), m_port(port), m_socket(INVALID_SOCKET_VALUE),
      m_running(false), m_registered(false)
{
}

TCPClient::~TCPClient() {
    closeConnect();
}

bool TCPClient::connect(const std::string& name) {
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == INVALID_SOCKET_VALUE) {
        std::cerr << "Failed to create socket, error: " << lastSocketError() << std::endl;
        return false;
    }

    struct sockaddr_in serverAddr;
    if (!resolveHost(m_host, m_port, serverAddr)) {
        std::cerr << "Failed to resolve host: " << m_host << std::endl;
        CLOSE_SOCKET(m_socket);
        m_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // 非阻塞连接以控制 connect 超时（系统默认可能阻塞数分钟）
#if defined(_WIN32)
    u_long mode = 1;
    ioctlsocket(m_socket, FIONBIO, &mode);
#else
    int flags = fcntl(m_socket, F_GETFL, 0);
    fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);
#endif

    char resolved_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &serverAddr.sin_addr, resolved_ip, sizeof(resolved_ip));
    std::cout << "connecting " << m_host << ":" << m_port << "  resolved=" << resolved_ip << std::endl;

    int rc = ::connect(m_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    bool connected = false;
    if (rc == 0) {
        connected = true;
    }
    else {
        // 非阻塞 connect 通常返回 -1，errno/WSAGetLastError 为 EINPROGRESS/WSAEWOULDBLOCK
#if defined(_WIN32)
        int err = WSAGetLastError();
        bool inProgress = (err == WSAEWOULDBLOCK);
#else
        bool inProgress = (errno == EINPROGRESS);
#endif
        if (inProgress) {
            fd_set writefds;
            FD_ZERO(&writefds);
            FD_SET(m_socket, &writefds);
            struct timeval tv;
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            int sel = select((int)m_socket + 1, nullptr, &writefds, nullptr, &tv);
            if (sel > 0 && FD_ISSET(m_socket, &writefds)) {
                int soerr = 0;
                socklen_t sl = sizeof(soerr);
                if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char*)&soerr, &sl) == 0 && soerr == 0) {
                    connected = true;
                }
                else {
                    std::cerr << "connect failed (SO_ERROR): " << soerr << std::endl;
                }
            }
            else {
                std::cerr << "connect timeout (10s)" << std::endl;
            }
        }
        else {
            std::cerr << "connect failed: " << lastSocketError() << std::endl;
        }
    }

    // 恢复阻塞模式
#if defined(_WIN32)
    mode = 0;
    ioctlsocket(m_socket, FIONBIO, &mode);
#else
    fcntl(m_socket, F_SETFL, flags);
#endif

    if (!connected) {
        CLOSE_SOCKET(m_socket);
        m_socket = INVALID_SOCKET_VALUE;
        return false;
    }

    // 仅对 send 设超时（connect 上面已用 select 控制了）
    // 注意：不设 SO_RCVTIMEO，长连接 recv 需无限期阻塞等待对端消息，否则超时会被误判为断连
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    if (setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR_MSG) {
        std::cerr << "setsockopt(SO_SNDTIMEO) failed: " << lastSocketError() << std::endl;
    }

    m_name = name;
    m_running = true;
    m_registered = false;

    // 启动接收线程
    m_recvThread = std::thread(&TCPClient::receiveMessages, this);

    // 启动心跳线程（30 秒间隔）
    m_heartbeatThread = std::thread(&TCPClient::heartbeatLoop, this);

    // 注册客户端
    if (!register_()) {
        std::cerr << "register failed: sendMessage error" << std::endl;
        return false;
    }
    return true;
}

bool TCPClient::register_() {
    Message msg(MessageType::REGISTER, { {"name", m_name} });
    return sendMessage(msg);
}

bool TCPClient::sendMessage(const Message& msg) {
    if (m_socket == INVALID_SOCKET_VALUE) return false;
    std::string data = msg.toJson() + "\n";
    const char* buf = data.c_str();
    int total = 0;
    int len = static_cast<int>(data.size());

    // 心跳线程 + 其他调 sendXXX 的线程会并发调用 sendMessage。
    // sendMessage 使用循环部分发送，必须加锁避免两条 JSON 消息在 socket 上交错。
    std::lock_guard<std::mutex> lock(m_sendMutex);
    while (total < len) {
        int sent = ::send(m_socket, buf + total, len - total, 0);
        if (sent == SOCKET_ERROR_MSG || sent <= 0) return false;
        total += sent;
    }
    return total == len;
}

void TCPClient::getClients() {
    Message msg(MessageType::GET_CLIENTS, {});
    if (!sendMessage(msg)) {
        std::cerr << "getClients: send failed" << std::endl;
        return;
    }
    std::cout << "get user list..." << std::endl;
}

bool TCPClient::sendPrivateMessage(const std::string& target, const std::string& content) {
    Message msg(MessageType::SEND_MESSAGE, { {"content", content} }, "", target);
    if (!sendMessage(msg)) {
        std::cerr << "msg send failed: " << target << std::endl;
        return false;
    }
    std::cout << "msg sent to " << target << std::endl;
    return true;
}

bool TCPClient::broadcast(const std::string& content) {
    Message msg(MessageType::BROADCAST, { {"content", content} });
    if (!sendMessage(msg)) {
        std::cerr << "broadcast send failed" << std::endl;
        return false;
    }
    std::cout << "broadcast sent" << std::endl;
    return true;
}

void TCPClient::receiveMessages() {
    const int BUFFER_SIZE = 4096;
    std::vector<char> buffer(BUFFER_SIZE);
    std::string leftover;
    int badLines = 0;         // 连续解析失败计数
    const int BAD_LINES_MAX = 5;
    bool need_exit = false;

    while (m_running.load() && !need_exit) {
        int received = recv(m_socket, buffer.data(), BUFFER_SIZE - 1, 0);
        if (received <= 0) {
            if (received == 0) {
                std::cout << "\nserver closed connect" << std::endl;
            }
            else {
                std::cout << "\nrecive msg err: " << lastSocketError() << std::endl;
            }
            break;
        }

        buffer[received] = '\0';
        std::string data(buffer.data(), received);
        leftover += data;

        size_t pos;
        while (!need_exit && (pos = leftover.find('\n')) != std::string::npos) {
            std::string line = leftover.substr(0, pos);
            leftover.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            // 协议不匹配快速识别：如果前几行是 HTTP 响应/请求头，立即断连
            if (badLines == 0 && (
                (line.rfind("HTTP/", 0) == 0) ||   // HTTP/1.1 400 Bad Request
                (line.rfind("GET ", 0) == 0) ||     // GET / HTTP/1.1
                (line.rfind("POST ", 0) == 0) ||    // POST / HTTP/1.1
                (line.rfind("Content-Type:", 0) == 0) ||
                (line.rfind("Content-Length:", 0) == 0)
            )) {
                std::cerr << utf8ToGbk("协议不匹配：对端返回的是 HTTP 报文，不是本项目的 JSON 行协议。")
                          << std::endl;
                std::cerr << utf8ToGbk("请确认端口是否映射到了 TCP服务端口(默认8180)，而不是 HTTP 服务端口(默认8181)。")
                          << std::endl;
                std::cerr << utf8ToGbk("收到的首行: ") << utf8ToGbk(line.substr(0, 120)) << std::endl;
                // 强制退出接收循环
                m_running = false;
                need_exit = true;
                break;
            }

            std::cout << utf8ToGbk(line) << std::endl;
            Message msg = Message::fromJson(line);
            if (msg.isValid()) {
                badLines = 0;
                handleMessage(msg);
            } else {
                ++badLines;
                if (badLines >= BAD_LINES_MAX) {
                    std::cerr << utf8ToGbk("连续多条消息解析失败，疑似协议不匹配，主动断开连接。") << std::endl;
                    m_running = false;
                    need_exit = true;
                    break;
                }
            }
        }
    }

    m_running = false;
    m_registered = false;
    std::cout << "\nserver disconnect" << std::endl;
}

void TCPClient::handleMessage(const Message& msg) {
    if (msg.type == MessageType::GET_CLIENTS) {
        auto clients = msg.data.value("clients", json::array());
        std::lock_guard<std::mutex> lock(m_mutex);
        m_clientsList.clear();
        for (const auto& c : clients) {
            m_clientsList.push_back(c.get<std::string>());
        }
        std::cout << "\ncur user (" << m_clientsList.size() << "): ";
        if (m_clientsList.empty()) {
            std::cout << "no user";
        }
        else {
            for (size_t i = 0; i < m_clientsList.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << utf8ToGbk(m_clientsList[i]);
            }
        }
        std::cout << std::endl;
    }
    else if (msg.type == MessageType::RESPONSE) {
        bool success = msg.data.value("success", false);
        std::string message = msg.data.value("message", "");
        if (success) {
            // 注册成功响应（message 是 UTF-8，源码字面量也是 UTF-8，字符串比较正确）
            if (!m_registered.load() && message.rfind("欢迎", 0) == 0) {
                m_registered = true;
            }
            if (!message.empty()) {
                std::cout << "\n " << utf8ToGbk(message) << std::endl;
            }
            // 检查是否包含客户端列表
            if (msg.data.contains("data")) {
                auto dataObj = msg.data["data"];
                if (dataObj.contains("clients")) {
                    auto clients = dataObj["clients"];
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_clientsList.clear();
                    for (const auto& c : clients) {
                        m_clientsList.push_back(c.get<std::string>());
                    }
                    std::cout << "cur user: ";
                    if (m_clientsList.empty()) {
                        std::cout << "no user";
                    }
                    else {
                        for (size_t i = 0; i < m_clientsList.size(); ++i) {
                            if (i > 0) std::cout << ", ";
                            std::cout << utf8ToGbk(m_clientsList[i]);
                        }
                    }
                    std::cout << std::endl;
                }
            }
        }
        else {
            std::cout << "\n[err] " << (message.empty() ? "unknown err" : utf8ToGbk(message)) << std::endl;
        }
    }
    else if (msg.type == MessageType::PRIVATE_MESSAGE) {
        std::string content = msg.data.value("content", "");
        std::cout << "\n[private msg] " << utf8ToGbk(msg.sender) << ": " << utf8ToGbk(content) << std::endl;
        
        handlePrivateMessage(msg.sender, content);
    }
    else if (msg.type == MessageType::BROADCAST) {
        std::string content = msg.data.value("content", "");
        std::cout << "\n[broad msg] " << utf8ToGbk(msg.sender) << ": " << utf8ToGbk(content) << std::endl;
    }
}

void TCPClient::heartbeatLoop() {
    while (m_running.load()) {
        // 每 30 秒发一次心跳
        for (int i = 0; i < 30 && m_running.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!m_running.load()) return;
        }
        if (!m_running.load()) return;
        Message hb(MessageType::HEARTBEAT, {});
        if (!sendMessage(hb)) {
            std::cerr << "heartbeat send failed, connection may be lost" << std::endl;
            return;
        }
    }
}

void TCPClient::closeConnect() {
    if (!m_running.exchange(false)) {
        // 已关闭，但线程可能还在 join
    }
    if (m_socket != INVALID_SOCKET_VALUE) {
        CLOSE_SOCKET(m_socket);
        m_socket = INVALID_SOCKET_VALUE;
    }
    if (m_recvThread.joinable()) {
        m_recvThread.join();
    }
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.join();
    }
    m_registered = false;
}

// 辅助函数实现
std::pair<std::string, int> parseServerAddress(const std::string& addrStr) {
    std::string host;
    int port = 8180;

    // 移除协议前缀
    size_t pos = addrStr.find("://");
    std::string temp = (pos != std::string::npos) ? addrStr.substr(pos + 3) : addrStr;

    // 查找端口分隔符
    pos = temp.find(':');
    if (pos != std::string::npos) {
        host = temp.substr(0, pos);
        try {
            port = std::stoi(temp.substr(pos + 1));
        }
        catch (...) {
            std::cerr << "port err" << std::endl;
            return { "", -1 };
        }
    }
    else {
        host = temp;
    }

    // 验证端口范围
    if (port <= 0 || port > 65535) {
        std::cerr << "port out of range: " << port << std::endl;
        return { "", -1 };
    }

    return { host, port };
}

void printHelp() {
    std::cout << R"(
command:
  /list          - get user
  /msg username content - send content to user
  /broad content  - broad content
  /help          - help
  /quit          - quit
)" << std::endl;
}

bool testConnection(const std::string& host, int port) {
    socket_t testSock = socket(AF_INET, SOCK_STREAM, 0);
    if (testSock == INVALID_SOCKET_VALUE) {
        return false;
    }

    struct sockaddr_in addr;
    if (!resolveHost(host, port, addr)) {
        CLOSE_SOCKET(testSock);
        return false;
    }

    // 非阻塞 + select 控制 connect 超时，避免长时间阻塞
#if defined(_WIN32)
    u_long mode = 1;
    ioctlsocket(testSock, FIONBIO, &mode);
#else
    int flags = fcntl(testSock, F_GETFL, 0);
    fcntl(testSock, F_SETFL, flags | O_NONBLOCK);
#endif

    int rc = ::connect(testSock, (struct sockaddr*)&addr, sizeof(addr));
    bool result = false;
    if (rc == 0) {
        result = true;
    }
    else {
#if defined(_WIN32)
        bool inProgress = (WSAGetLastError() == WSAEWOULDBLOCK);
#else
        bool inProgress = (errno == EINPROGRESS);
#endif
        if (inProgress) {
            fd_set writefds;
            FD_ZERO(&writefds);
            FD_SET(testSock, &writefds);
            struct timeval tv;
            tv.tv_sec = 3;
            tv.tv_usec = 0;
            int sel = select((int)testSock + 1, nullptr, &writefds, nullptr, &tv);
            if (sel > 0 && FD_ISSET(testSock, &writefds)) {
                int soerr = 0;
                socklen_t sl = sizeof(soerr);
                if (getsockopt(testSock, SOL_SOCKET, SO_ERROR, (char*)&soerr, &sl) == 0 && soerr == 0) {
                    result = true;
                }
            }
        }
    }

    // 简化：直接关闭，不再 shutdown+drain（避免对端大量数据导致阻塞）
    CLOSE_SOCKET(testSock);
    return result;
}

// 单例：使用 unique_ptr 避免泄漏，提供显式关闭函数
namespace {
    std::unique_ptr<TCPClient> g_instance;
    std::mutex g_instance_mutex;
}

TCPClient* TcpClientInstance(const char* address, const char* name)
{
    std::lock_guard<std::mutex> lock(g_instance_mutex);
    if (!g_instance) {
        std::string addressInput = address;
        std::string host;
        int port = 8180;

        auto [h, p] = parseServerAddress(addressInput);
        if (!h.empty() && p > 0) {
            host = h;
            port = p;
        } else {
            std::cout << "address err" << std::endl;
            return nullptr;
        }

        g_instance = std::make_unique<TCPClient>(host, port);
        if (!g_instance->connect(name)) {
            std::cout << "connect err: " << host << ":" << port << std::endl;
            g_instance.reset();
            return nullptr;
        }

        std::cout << "connect success " << host << ":" << port << " as " << name << std::endl;

        printHelp();
    }

    return g_instance.get();
}

void TcpClientCloseInstance() {
    std::lock_guard<std::mutex> lock(g_instance_mutex);
    if (g_instance) {
        g_instance->closeConnect();
        g_instance.reset();
    }
}

void TcpClientSendMsg(const char* name, const char* msg)
{
    std::lock_guard<std::mutex> lock(g_instance_mutex);
    if (g_instance) {
        std::string content = msg;
        if(name == NULL)
        {
            g_instance->broadcast(content);
        }
    }
}

int main(int argc, char* argv[])
{
#if defined(_WIN32)
    //windows 网络应用初始化
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
#endif

    TcpClientInstance(argv[1], argv[2]);
    getchar();
    TcpClientCloseInstance();
    
#if defined(_WIN32)
    WSACleanup();
#endif

    return 0;
}