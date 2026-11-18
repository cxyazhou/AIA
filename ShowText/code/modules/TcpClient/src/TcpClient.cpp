#include "TcpClient.h"
#include <iostream>
#include <cstring>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <cstdlib>

extern void AIAShowTips(const char* text);

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
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

// 日志：打印到控制台（中文自动转 GBK 适配 Windows 默认控制台）
static void logLine(const char* tag, std::ostream& os, const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#if defined(WIN32) || defined(WIN64)
    localtime_s(&tm_buf, &now_time);
#else
    localtime_r(&now_time, &tm_buf);
#endif
    os << "[" << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "] " << tag << " "
       << utf8ToGbk(msg) << std::endl;
}
static void log_error(const std::string& msg) { logLine("[CLIENT][ERR]", std::cerr, msg); }
static void log_warn(const std::string& msg)  { logLine("[CLIENT][WARN]", std::cerr, msg); }

// ========== 工具 ==========
static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// 解析能力列表字符串："听、说、看" / "听,说,看"（支持多种分隔符）
static std::set<std::string> split_caps(const std::string& s) {
    std::set<std::string> out;
    if (s.empty()) return out;
    std::string cur;
    for (size_t i = 0; i <= s.size(); ++i) {
        char ch = (i == s.size()) ? '\0' : s[i];
        bool sep = (i == s.size()) ||
            (ch == '、') || (ch == '，') || (ch == ',') ||
            (ch == ';') || (ch == '；') ||
            (ch == ' ') || (ch == '\t') ||
            (ch == '/') || (ch == '|');
        if (sep) {
            std::string t = trim(cur);
            if (!t.empty()) out.insert(t);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    return out;
}

// base64 编解码：音频/视频二进制数据以 base64 字符串放进 JSON 字段
static const char* B64CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static std::string base64_encode(const unsigned char* data, size_t len) {
    std::string out;
    if (!data || len == 0) return out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    while (i < len) {
        unsigned int a = i < len ? (unsigned char)data[i++] : 0;
        unsigned int b = i < len ? (unsigned char)data[i++] : 0;
        unsigned int c = i < len ? (unsigned char)data[i++] : 0;
        unsigned int triple = (a << 16) + (b << 8) + c;
        out.push_back(B64CHARS[(triple >> 18) & 0x3F]);
        out.push_back(B64CHARS[(triple >> 12) & 0x3F]);
        out.push_back(B64CHARS[(triple >> 6)  & 0x3F]);
        out.push_back(B64CHARS[ triple        & 0x3F]);
    }
    size_t pad = (3 - len % 3) % 3;
    for (size_t k = 0; k < pad; ++k) out[out.size() - 1 - k] = '=';
    return out;
}
static std::vector<unsigned char> base64_decode(const std::string& s) {
    std::vector<unsigned char> out;
    if (s.empty()) return out;
    int T[256];
    std::memset(T, -1, sizeof(T));
    for (int i = 0; i < 64; ++i) T[(unsigned char)B64CHARS[i]] = i;
    int val = 0, vb = -8;
    for (char c : s) {
        if (T[(unsigned char)c] == -1) {
            if (c == '=') break;
            continue;
        }
        val = (val << 6) + T[(unsigned char)c];
        vb += 6;
        if (vb >= 0) {
            out.push_back(static_cast<unsigned char>((val >> vb) & 0xFF));
            vb -= 8;
        }
    }
    return out;
}

// Message 类实现
Message::Message() {}
Message::Message(const std::string& t, const json& d, const std::string& s, const std::string& tg)
    : type(t), data(d), sender(s), target(tg) {}
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
        msg.type   = j.value("type", "");
        msg.data   = j.value("data", json::object());
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

// 域名/IP 解析为 sockaddr_in（IPv4，TCP）
bool resolveHost(const std::string& host, int port, sockaddr_in& out) {
    std::memset(&out, 0, sizeof(out));
    out.sin_family = AF_INET;
    out.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &out.sin_addr) == 1) return true;

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    int err = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (err != 0 || res == nullptr) {
        if (res) freeaddrinfo(res);
#if defined(WIN32) || defined(WIN64)
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

std::pair<std::string, int> parseServerAddress(const std::string& addrStr) {
    std::string host; int port = 8180;
    size_t pos = addrStr.find("://");
    std::string temp = (pos != std::string::npos) ? addrStr.substr(pos + 3) : addrStr;
    pos = temp.find(':');
    if (pos != std::string::npos) {
        host = temp.substr(0, pos);
        try { port = std::stoi(temp.substr(pos + 1)); }
        catch (...) { std::cerr << "port err" << std::endl; return { "", -1 }; }
    } else host = temp;
    if (port <= 0 || port > 65535) { std::cerr << "port out of range: " << port << std::endl; return { "", -1 }; }
    return { host, port };
}

// ==================== TCPClient ====================

TCPClient::TCPClient(const std::string& host, int port)
    : m_host(host), m_port(port), m_socket(INVALID_SOCKET_VALUE),
      m_running(false), m_registered(false), m_userClose(false), m_cmdContext(nullptr)
{}

TCPClient::~TCPClient() { closeConnect(); }

void TCPClient::setCommandHandler(AIACommandHandler handler, void* context) {
    m_cmdHandler = std::move(handler);
    m_cmdContext = context;
}

// 创建 socket 并建立 TCP 连接（非阻塞 connect + select 超时控制），成功后 m_socket 已就绪。
// 不启动线程、不发送注册消息，供首次连接与重连复用。
bool TCPClient::establishSocket() {
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == INVALID_SOCKET_VALUE) {
        m_lastError = "Failed to create socket: " + lastSocketError();
        std::cerr << "Failed to create socket, error: " << lastSocketError() << std::endl;
        return false;
    }
    struct sockaddr_in serverAddr;
    if (!resolveHost(m_host, m_port, serverAddr)) {
        std::cerr << "Failed to resolve host: " << m_host << std::endl;
        CLOSE_SOCKET(m_socket); m_socket = INVALID_SOCKET_VALUE; return false;
    }

    // 非阻塞 connect（带 10 秒超时）
#if defined(WIN32) || defined(WIN64)
    u_long mode = 1; ioctlsocket(m_socket, FIONBIO, &mode);
#else
    int flags = fcntl(m_socket, F_GETFL, 0);
    fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);
#endif
    int rc = ::connect(m_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    bool connected = (rc == 0);
    if (!connected) {
#if defined(WIN32) || defined(WIN64)
        bool inProgress = (WSAGetLastError() == WSAEWOULDBLOCK);
#else
        bool inProgress = (errno == EINPROGRESS);
#endif
        if (inProgress) {
            fd_set writefds; FD_ZERO(&writefds); FD_SET(m_socket, &writefds);
            struct timeval tv; tv.tv_sec = 10; tv.tv_usec = 0;
            int sel = select((int)m_socket + 1, nullptr, &writefds, nullptr, &tv);
            if (sel > 0 && FD_ISSET(m_socket, &writefds)) {
                int soerr = 0; socklen_t sl = sizeof(soerr);
                if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char*)&soerr, &sl) == 0 && soerr == 0)
                    connected = true;
            }
        }
    }
#if defined(WIN32) || defined(WIN64)
    mode = 0; ioctlsocket(m_socket, FIONBIO, &mode); // 恢复阻塞模式
#else
    fcntl(m_socket, F_SETFL, flags);
#endif
    if (!connected) { CLOSE_SOCKET(m_socket); m_socket = INVALID_SOCKET_VALUE; return false; }

    struct timeval timeout; timeout.tv_sec = 10; timeout.tv_usec = 0;
    if (setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR_MSG) {
        std::cerr << "setsockopt(SO_SNDTIMEO) failed: " << lastSocketError() << std::endl;
    }
    return true;
}

// 连接并注册（上报支持的命令）：建立 socket 后启动接收/心跳线程并发送注册消息
bool TCPClient::connectWithCapabilities(const std::string& name,
                                        const ClientCapabilityConfig& caps,
                                        const std::string& password)
{
    m_name = name;
    m_password = password;
    m_caps = caps;
    m_lastError.clear();
    m_userClose = false;

    if (!establishSocket()) return false;

    m_running = true; m_registered = false;
    m_recvThread = std::thread(&TCPClient::receiveMessages, this);
    m_heartbeatThread = std::thread(&TCPClient::heartbeatLoop, this);

    if (!register_()) {
        std::cerr << "register failed: sendMessage error" << std::endl;
        closeConnect();   // 清理已启动的线程与 socket
        return false;
    }
    return true;
}

// 发送注册消息，上报名称与支持的命令（cmd 字段，逗号分割）
bool TCPClient::register_() {
    json data;
    data[FieldName::Name] = m_name;

    std::string cmds;
    for (const auto& c : m_caps.executable_commands) {
        if (!cmds.empty()) cmds += ",";
        cmds += c;
    }
    data["cmd"] = cmds;

    if (!m_password.empty()) data["password"] = m_password;

    Message msg(AIACommand::REGISTER, data, m_name, "server");
    return sendMessage(msg);
}

// 循环发送直到完整写出；失败返回 false
bool TCPClient::sendMessage(const Message& msg) {
    if (m_socket == INVALID_SOCKET_VALUE) return false;
    std::string data = msg.toJson() + "\n";
    const char* buf = data.c_str();
    int total = 0;
    int len = static_cast<int>(data.size());
    std::lock_guard<std::mutex> lock(m_sendMutex);
    while (total < len) {
        int sent = ::send(m_socket, buf + total, len - total, 0);
        if (sent == SOCKET_ERROR_MSG || sent <= 0) return false;
        total += sent;
    }
    return total == len;
}

// ========== 消息发送接口 ==========

void TCPClient::getClients() {
    Message msg(AIACommand::GET_CLIENTS, {});
    if (!sendMessage(msg)) std::cerr << "getClients: send failed" << std::endl;
}

bool TCPClient::sendPrivateMessage(const std::string& target, const std::string& content) {
    Message msg(AIACommand::SEND_MESSAGE, { {"content", content} }, m_name, target);
    if (!sendMessage(msg)) { std::cerr << "msg send failed: " << target << std::endl; return false; }
    return true;
}

bool TCPClient::broadcast(const std::string& content) {
    Message msg(AIACommand::BROADCAST, { {"content", content} }, m_name, "all_users");
    if (!sendMessage(msg)) { std::cerr << "broadcast send failed" << std::endl; return false; }
    return true;
}

// ========== 接收循环：按行拆分 JSON 消息并分发 ==========
// 连接被动断开（recv<=0）且非用户主动关闭时，休眠 10 秒后自动重连。
void TCPClient::receiveMessages() {
    const int BUFFER_SIZE = 4096;
    std::vector<char> buffer(BUFFER_SIZE);
    std::string leftover;
    int badLines = 0;
    const int BAD_LINES_MAX = 5;

    // 外层循环：连接断开后在此重连
    while (!m_userClose.load()) {
        bool need_exit = false;   // 协议不匹配等致命错误，直接退出不重连

        // 内层接收循环
        while (m_running.load() && !need_exit) {
            int received = recv(m_socket, buffer.data(), BUFFER_SIZE - 1, 0);
            if (received <= 0) {
                break;
            }
            buffer[received] = '\0';
            leftover.append(buffer.data(), received);

            size_t pos;
            while (!need_exit && (pos = leftover.find('\n')) != std::string::npos) {
                std::string line = leftover.substr(0, pos);
                leftover.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;

                // 对端返回 HTTP 报文说明连到了 HTTP 端口，协议不匹配，直接断开
                if (badLines == 0 && (
                    (line.rfind("HTTP/", 0) == 0) ||
                    (line.rfind("GET ", 0) == 0) ||
                    (line.rfind("POST ", 0) == 0) ||
                    (line.rfind("Content-Type:", 0) == 0) ||
                    (line.rfind("Content-Length:", 0) == 0)
                )) {
                    std::cerr << utf8ToGbk("协议不匹配：对端返回的是 HTTP 报文，不是本项目的 JSON 行协议。") << std::endl;
                    std::cerr << utf8ToGbk("请确认端口是否映射到了 TCP服务端口(默认8180)，而不是 HTTP 服务端口(默认8181)。") << std::endl;
                    m_running = false; need_exit = true; break;
                }

                Message msg = Message::fromJson(line);
                if (msg.isValid()) {
                    badLines = 0;
                    handleMessage(msg);
                } else {
                    ++badLines;
                    if (badLines >= BAD_LINES_MAX) {
                        std::cerr << utf8ToGbk("连续多条消息解析失败，疑似协议不匹配，主动断开连接。") << std::endl;
                        m_running = false; need_exit = true; break;
                    }
                }
            }
        }

        badLines = 0;
        leftover.clear();

        // 用户主动关闭 或 协议不匹配：直接退出，不重连
        if (m_userClose.load() || need_exit) {
            m_running = false; m_registered = false;
            return;
        }

        // 连接被动断开：清理旧 socket，等待旧心跳线程退出，进入重连循环
        m_running = false; m_registered = false;
        if (m_socket != INVALID_SOCKET_VALUE) {
            CLOSE_SOCKET(m_socket);
            m_socket = INVALID_SOCKET_VALUE;
        }
        if (m_heartbeatThread.joinable()) m_heartbeatThread.join();

        while (!m_userClose.load()) {
            std::cerr << utf8ToGbk("连接断开，10 秒后重连服务器...") << std::endl;
            for (int i = 0; i < 10 && !m_userClose.load(); ++i)
                std::this_thread::sleep_for(std::chrono::seconds(1));
            if (m_userClose.load()) return;

            if (!establishSocket()) continue;   // 连接失败，继续重试

            m_running = true; m_registered = false;
            m_heartbeatThread = std::thread(&TCPClient::heartbeatLoop, this);
            if (!register_()) {
                std::cerr << utf8ToGbk("重连后注册失败，继续重试") << std::endl;
                m_running = false;
                if (m_heartbeatThread.joinable()) m_heartbeatThread.join();
                if (m_socket != INVALID_SOCKET_VALUE) { CLOSE_SOCKET(m_socket); m_socket = INVALID_SOCKET_VALUE; }
                continue;
            }
            std::cerr << utf8ToGbk("重连成功") << std::endl;
            break;   // 回到外层循环，重新进入接收
        }
    }
    m_running = false; m_registered = false;
}

// 更新在线客户端列表（GET_CLIENTS 响应 / RESPONSE 附带的 clients）
void TCPClient::updateClientsList(const json& clients) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_clientsList.clear();
    for (const auto& c : clients) m_clientsList.push_back(c.get<std::string>());
}

static void announce(const std::string& text) {
    AIAShowTips(text.c_str());
}

void TCPClient::handleMessage(const Message& msg) {
    // 服务器下发的命令先交给上层回调；回调返回 true 表示已消费，否则走默认实现
    bool isServerCommand = (msg.type == ExecutableCommand::TEXT_DISPLAY);

    if (isServerCommand) {
        AIAShowTips((const char*)msg.data.value("文本", "").c_str());
    }

    if (msg.type == AIACommand::GET_CLIENTS) {
        updateClientsList(msg.data.value("clients", json::array()));

    } else if (msg.type == AIACommand::RESPONSE) {
        bool success = msg.data.value("success", false);
        std::string message = msg.data.value("message", "");
        if (success) {
            if (!m_registered.load() && message.rfind("欢迎", 0) == 0) {
                m_registered = true; m_lastError.clear();
            } else if (!m_registered.load() && message.find("已存在") != std::string::npos) {
                log_warn("注册失败：" + message);
                closeConnect();
                return;
            }
            if (msg.data.contains("data") && msg.data["data"].contains("clients")) {
                updateClientsList(msg.data["data"]["clients"]);
            }
        } else {
            m_lastError = message;
            std::cerr << "[err] " << (message.empty() ? "unknown err" : message) << std::endl;
            // 服务器返回"名称已存在"或"连接数已达上限"时，直接退出程序
            if (message.find("已存在") != std::string::npos ||
                message.find("已达上限") != std::string::npos) {
                log_error("服务器拒绝连接：" + message + "，程序退出");
                // 直接终止进程，避免在接收线程内 closeConnect 触发 m_recvThread.join() 自连接死锁
                std::_Exit(1);
            }
        }

    } else if (msg.type == AIACommand::PRIVATE_MESSAGE) {
        std::string content = msg.data.value("content", "");
        announce("收到 " + msg.sender + " 私信：" + content);

    } else if (msg.type == AIACommand::BROADCAST) {
        std::string content = msg.data.value("content", "");
        announce("收到 " + msg.sender + " 广播：" + content);

    }
}

// ========== 心跳与关闭 ==========

// 每 30 秒向服务器发送一次心跳
void TCPClient::heartbeatLoop() {
    while (m_running.load()) {
        for (int i = 0; i < 30 && m_running.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!m_running.load()) return;
        }
        if (!m_running.load()) return;
        Message hb(AIACommand::HEARTBEAT, {});
        if (!sendMessage(hb)) {
            std::cerr << "heartbeat send failed, connection may be lost" << std::endl;
            // 关闭 socket，让接收线程的 recv 立即返回，触发自动重连
            if (m_socket != INVALID_SOCKET_VALUE) {
                CLOSE_SOCKET(m_socket);
                m_socket = INVALID_SOCKET_VALUE;
            }
            return;
        }
    }
}

void TCPClient::closeConnect() {
    m_userClose = true;   // 通知接收线程不要再自动重连
    m_running = false;
    if (m_socket != INVALID_SOCKET_VALUE) {
        CLOSE_SOCKET(m_socket);
        m_socket = INVALID_SOCKET_VALUE;
    }
    if (m_recvThread.joinable())      m_recvThread.join();
    if (m_heartbeatThread.joinable()) m_heartbeatThread.join();
    m_registered = false;
}

// ==================== 全局单例接口 ====================

namespace {
    std::unique_ptr<TCPClient> g_instance;
    std::mutex g_instance_mutex;
}

TCPClient* TcpClientInstance(const char* address, const char* name,
                             const char* password,
                             const char* commandsStr)
{
    std::string addressInput = address ? address : "";
    std::string host; int port = 8180;
    auto [h, p] = parseServerAddress(addressInput);
    if (!h.empty() && p > 0) { host = h; port = p; }
    else { std::cerr << "address err" << std::endl; return nullptr; }

    ClientCapabilityConfig caps;
    if (commandsStr) {
        for (auto& c : split_caps(commandsStr)) caps.addCommand(c);
    }

    std::string pwd = password ? password : "";
    std::string nm  = name ? name : "";

    // 连接服务器失败则休眠 10 秒后重试，直到连接成功
    while (true) {
        {
            std::lock_guard<std::mutex> lock(g_instance_mutex);
            if (g_instance) return g_instance.get();

            g_instance = std::make_unique<TCPClient>(host, port);
            if (g_instance->connectWithCapabilities(nm, caps, pwd)) {
                return g_instance.get();
            }
            std::cerr << "connect err: " << host << ":" << port << std::endl;
            g_instance.reset();
        }
        std::cerr << "10 秒后重试连接服务器..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

void TcpClientCloseInstance() {
    std::lock_guard<std::mutex> lock(g_instance_mutex);
    if (g_instance) { g_instance->closeConnect(); g_instance.reset(); }
}

TCPClient* TcpClientGet() {
    std::lock_guard<std::mutex> lock(g_instance_mutex);
    return g_instance.get();
}

void TcpClientSendMsg(const char* name, const char* msg) {
    std::lock_guard<std::mutex> lock(g_instance_mutex);
    if (g_instance) {
        std::string content = msg ? msg : "";
        if (name == NULL) g_instance->broadcast(content);
        else              g_instance->sendPrivateMessage(name, content);
    }
}

