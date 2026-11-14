#include "Server.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cstring>
#include <cstdio>
#include <set>

extern int AIAVIPCheck();

// ==================== 日志工具 ====================
#if defined(WIN32) || defined(WIN64)
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

// 多线程环境下保证每条日志整行输出（控制台输出统一 UTF-8，main 入口已设 65001 代码页）
static void logLine(const char* level, std::ostream& os, const std::string& msg) {
    static std::mutex log_mutex;
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#if defined(WIN32) || defined(WIN64)
    localtime_s(&tm_buf, &now_time);
#else
    localtime_r(&now_time, &tm_buf);
#endif
    std::lock_guard<std::mutex> lock(log_mutex);
    os << "[" << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "] [" << level << "] " << utf8ToGbk(msg) << std::endl;
}

static void log_info(const std::string& msg)  { logLine("INFO", std::cout, msg); }
static void log_error(const std::string& msg) { logLine("ERROR", std::cerr, msg); }
static void log_warn(const std::string& msg)  { logLine("WARN", std::cout, msg); }

static std::string addr_to_string(const sockaddr_in& addr) {
    std::ostringstream oss;
    oss << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port);
    return oss.str();
}

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// Message 类实现
Message::Message() {}

Message::Message(const std::string& t, const json& d,
    const std::string& s, const std::string& tg)
    : type(t), data(d), sender(s), target(tg) {
}

std::string Message::toJson() const {
    json j;
    j["id"] = id;
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
        msg.id = j.value("id", (uint64_t)0);
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

// ==================== ClientHandler 实现 ====================
ClientHandler::ClientHandler(socket_t sock, const sockaddr_in& addr, TCP_HTTP_Server* srv)
    : socket_(sock), address_(addr), server_(srv), running(false), closed_(false), joined_(false) {}

ClientHandler::~ClientHandler() {
    close();
    if (thread_.joinable() && !joined_.load(std::memory_order_acquire)) {
        thread_.detach();
    }
}

void ClientHandler::start() {
    running = true;
    auto self = shared_from_this();
    thread_ = std::thread([self]() {
        self->handle_client();
    });
}

void ClientHandler::handle_client() {
    try {
        while (running) {
            char recv_buf[4096];
            int n = recv(socket_, recv_buf, sizeof(recv_buf) - 1, 0);
            if (n <= 0) {
                break;
            }
            recv_buf[n] = 0;
            buffer += std::string(recv_buf, recv_buf + n);

            size_t nl_pos;
            while ((nl_pos = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, nl_pos);
                buffer.erase(0, nl_pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (trim(line).empty()) {
                    continue;
                }
                Message msg = Message::fromJson(line);
                if (!msg.isValid()) {
                    log_warn("server 无法解析的消息: " + line.substr(0, 100));
                    continue;
                }
                std::string who = name.empty() ? addr_to_string(address_) : name;
                process_message(msg);
            }
        }
    } catch (const std::exception& e) {
        log_error(std::string("处理客户端消息时出错: ") + e.what());
    } catch (...) {
        log_error("处理客户端消息时发生未知错误");
    }
    close();
}

void ClientHandler::process_message(const Message& msg) {
    if (msg.type == AIACommand::REGISTER) {
        // 名称：支持 "客户端名称" / "name" 两种 key
        name.clear();
        for (const char* k : { FieldName::Name, "name", "client_name", "clientName" }) {
            if (msg.data.contains(k) && msg.data[k].is_string()) {
                name = msg.data.value(k, std::string());
                if (!name.empty()) break;
            }
        }

        // 解析可执行命令列表：cmd 字段，逗号分割，如 "录制音频,录制视频,显示文本"
        caps.executable_commands.clear();
        {
            std::istringstream iss(msg.data.value("cmd", std::string()));
            std::string item;
            while (std::getline(iss, item, ',')) {
                std::string t = trim(item);
                if (!t.empty()) caps.executable_commands.insert(t);
            }
        }

        // 连接密码校验
        std::string provided_pwd;
        for (const char* k : {"password", "server_password", "serverPassword", "pwd", "连接密码"}) {
            if (msg.data.contains(k)) { provided_pwd = msg.data.value(k, ""); if (!provided_pwd.empty()) break; }
        }
        if (server_ && !server_->check_connect_password(provided_pwd)) {
            send_response(false, "密码错误");
            log_warn("客户端注册被拒：密码错误，name=" + name);
            close();
            return;
        }
        if (name.empty()) {
            send_response(false, "注册失败：未提供名称");
            log_warn("客户端注册失败：未提供名称");
            close();
            return;
        }
        
        auto self = shared_from_this();
        std::string error_reason;
        if (server_->register_client(self, error_reason)) {
            // 打印客户端支持的命令：[客户端名称]:[命令]
            send_response(true, "欢迎 " + name + "！");
        } else {
            send_response(false, "注册失败：" + error_reason);
            log_warn("客户端注册被拒：" + error_reason + " " + name + "，主动关闭连接");
            close();
            return;
        }

        // 收到客户端注册成功，打印[客户端名称]:[命令]
        std::string cmd_str;
        for (const auto& c : caps.executable_commands) {
            cmd_str += "[" + c + "]";
        }
        log_info("[" + name + "]注册成功，支持命令:" + cmd_str);

        return;
    } 

    // 收到客户端命令：[客户端名称]:[命令]
    log_info("[" + name + "]:[" + msg.type + "]");

    if (msg.type == AIACommand::GET_CLIENTS) {
        std::vector<std::string> list = server_->get_clients_list();
        json arr = json::array();
        for (const auto& n : list) arr.push_back(n);
        json data = { {"clients", arr} };
        Message resp(AIACommand::GET_CLIENTS, data, "server", name);
        send_message(resp);
    } else if (msg.type == AIACommand::SEND_MESSAGE) {
        std::string target = msg.target;
        std::string content = msg.data.value("content", "");
        if (!target.empty() && !content.empty()) {
            json d = { {"content", content} };
            Message pm(AIACommand::PRIVATE_MESSAGE, d, name, target);
            bool ok = server_->send_message_to_client(target, pm);
            if (!ok) send_response(false, "消息发送失败：客户端 " + target + " 不存在");
        } else {
            send_response(false, "消息发送失败：缺少目标或内容");
        }
    } else if (msg.type == AIACommand::BROADCAST) {
        std::string content = msg.data.value("content", "");
        if (!content.empty()) {
            json d = { {"content", content} };
            Message bm(AIACommand::BROADCAST, d, name, "all_users");
            server_->broadcast_message(bm, name);
        } else {
            send_response(false, "广播发送失败：缺少内容");
        }
    } else if (msg.type == AIACommand::HEARTBEAT) {
        // 心跳，无需处理
    }
    else if (msg.type == AIACommand::RESPONSE) {
        std::string client_name = msg.data.value(FieldName::Name, std::string());
        std::string response_text = msg.data.value(FieldName::Text, std::string());

        if (response_text.empty()) return;

        std::string text = client_name + "说" + response_text;
        server_->send_text_display(text);

        if (!server_->find_client_by_command(ExecutableCommand::PLAY_AUDIO).empty())
        {
            std::string tts_client = server_->find_client_by_command(ExecutableCommand::SPEECH_GENERATION);
            if (!tts_client.empty())
            {
                json req;
                req[FieldName::Text] = text;
                Message msg(ExecutableCommand::SPEECH_GENERATION, req, "server", tts_client);

                json result_audio;
                if (server_->send_and_wait(tts_client, msg, result_audio, 60)) {
                    server_->send_audio_play(result_audio);
                }
            }
        }
    } else {
        // 其余可执行命令结果（语音识别/人脸识别/语音生成/显示文本/播放音频/播放视频）：
        // 先检查是否有任务线程在等待此结果，有则交付并返回；无则走默认处理
        printf("id back:%lld\n", msg.id);
        if (server_ && server_->try_fulfill_pending(msg.id, msg.data)) {
            return;
        }
        if (msg.type == ExecutableCommand::RECORD_AUDIO) {
            if (server_) server_->handle_record_audio(name, msg.data);
        } else if (msg.type == ExecutableCommand::RECORD_VIDEO) {
            if (server_) server_->handle_record_video(name, msg.data);
        } else {
        }
    }
}

void ClientHandler::send_response(bool success, const std::string& message, const json& data) {
    json d = {
        {"success", success},
        {"message", message},
        {"data", data}
    };
    Message resp(AIACommand::RESPONSE, d, "server", name);
    if (!send_message(resp)) {
        log_warn("server 发送响应给 " + (name.empty() ? addr_to_string(address_) : name) + " 失败");
    }
}

bool ClientHandler::send_message(const Message& msg) {
    try {
        std::string json_str = msg.toJson() + "\n";
        int total = 0;
        int len = static_cast<int>(json_str.size());
        const char* buf = json_str.c_str();
        while (total < len) {
            int n = send(socket_, buf + total, len - total, 0);
            if (n == SOCKET_ERROR_MSG || n <= 0) return false;
            total += n;
        }
        return true;
    } catch (...) {
        log_error("客户端发送消息失败");
        return false;
    }
}

void ClientHandler::close() {
    bool expected = false;
    if (!closed_.compare_exchange_strong(expected, true)) return;
    running = false;
    if (!name.empty() && server_ && !server_->stopping.load()) {
        auto self = shared_from_this();
        server_->unregister_client(self);
    }
    if (socket_ != INVALID_SOCKET_VALUE) {
        CLOSE_SOCKET(socket_);
        socket_ = INVALID_SOCKET_VALUE;
    }
    if (server_ && !server_->stopping.load()) {
        auto self = shared_from_this();
        server_->detach_handler(self);
    }
}

// ==================== TCP_HTTP_Server 实现 ====================
static TCP_HTTP_Server* g_server = nullptr;

TCP_HTTP_Server::TCP_HTTP_Server(const std::string& h, int tcp_p, int http_p)
    : host(h), tcp_port(tcp_p), http_port(http_p),
      running(false), stopping(false),
      tcp_server_socket(INVALID_SOCKET_VALUE), http_server_socket(INVALID_SOCKET_VALUE),
      speech_thread_running_(false) {}

TCP_HTTP_Server::~TCP_HTTP_Server() {
    stop();
}

#if defined(WIN32) || defined(WIN64)
// 启动前清理占用指定端口的进程：用 netstat -ano | findstr 查 PID，taskkill /F 强制结束。
// 用于处理上次异常退出后遗留的 CLOSE_WAIT 连接导致端口仍被占用的情况。
// 杀完后等待端口释放（taskkill 是异步的，进程退出与 socket 释放需要一点时间）。
static void kill_processes_on_port(int port) {
    std::string portStr = std::to_string(port);
    auto collect_pids = [&]() {
        std::string cmd = "netstat -ano | findstr \"" + portStr + "\"";
        FILE* pipe = _popen(cmd.c_str(), "r");
        std::set<std::string> pids;
        if (!pipe) return pids;
        char buf[1024];
        while (fgets(buf, sizeof(buf), pipe)) {
            std::string line(buf);
            size_t end = line.find_last_not_of(" \t\r\n");
            if (end == std::string::npos) continue;
            size_t start = end;
            while (start > 0 && !std::isspace((unsigned char)line[start - 1])) --start;
            std::string pid = line.substr(start, end - start + 1);
            if (pid.empty() || pid == "0") continue;
            bool isNum = true;
            for (char c : pid) { if (!std::isdigit((unsigned char)c)) { isNum = false; break; } }
            if (!isNum) continue;
            pids.insert(pid);
        }
        _pclose(pipe);
        return pids;
    };

    std::set<std::string> pids = collect_pids();
    for (const auto& pid : pids) {
        log_info("端口 " + portStr + " 被 PID " + pid + " 占用，强制结束该进程");
        std::string killCmd = "taskkill /PID " + pid + " /F";
        FILE* kpipe = _popen(killCmd.c_str(), "r");
        if (kpipe) { char buf[1024]; while (fgets(buf, sizeof(buf), kpipe)) {} _pclose(kpipe); }
    }

    if (pids.empty()) return;   // 没有占用，直接返回

    // taskkill 异步，循环检测端口是否已释放，最多等待 3 秒
    for (int i = 0; i < 15; ++i) {
        if (collect_pids().empty()) {
            log_info("端口 " + portStr + " 已释放");
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    log_warn("等待 3 秒后端口 " + portStr + " 仍未释放，继续尝试 bind");
}
#endif

bool TCP_HTTP_Server::start() {
#if defined(WIN32) || defined(WIN64)
    // 启动前清理上次异常退出可能遗留的端口占用
    kill_processes_on_port(tcp_port);
    kill_processes_on_port(http_port);
#endif
    try {
        tcp_server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (tcp_server_socket == INVALID_SOCKET_VALUE) {
            log_error("创建 TCP socket 失败 " + lastSocketError());
            return false;
        }
        int opt = 1;
        if (setsockopt(tcp_server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) == SOCKET_ERROR_MSG) {
            log_warn("TCP setsockopt(SO_REUSEADDR) 失败 " + lastSocketError());
        }

        sockaddr_in tcp_addr;
        std::memset(&tcp_addr, 0, sizeof(tcp_addr));
        tcp_addr.sin_family = AF_INET;
        tcp_addr.sin_port = htons(tcp_port);
        if (host.empty() || host == "0.0.0.0") tcp_addr.sin_addr.s_addr = INADDR_ANY;
        else tcp_addr.sin_addr.s_addr = inet_addr(host.c_str());

        if (bind(tcp_server_socket, (sockaddr*)&tcp_addr, sizeof(tcp_addr)) == SOCKET_ERROR_MSG) {
            log_error("绑定 TCP 端口 " + std::to_string(tcp_port) + " 失败 " + lastSocketError());
            CLOSE_SOCKET(tcp_server_socket);
            tcp_server_socket = INVALID_SOCKET_VALUE;
            return false;
        }
        if (listen(tcp_server_socket, 5) == SOCKET_ERROR_MSG) {
            log_error("TCP listen 失败 " + lastSocketError());
            CLOSE_SOCKET(tcp_server_socket);
            tcp_server_socket = INVALID_SOCKET_VALUE;
            return false;
        }

        http_server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (http_server_socket == INVALID_SOCKET_VALUE) {
            log_error("创建 HTTP socket 失败 " + lastSocketError());
            CLOSE_SOCKET(tcp_server_socket);
            tcp_server_socket = INVALID_SOCKET_VALUE;
            return false;
        }
        if (setsockopt(http_server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) == SOCKET_ERROR_MSG) {
            log_warn("HTTP setsockopt(SO_REUSEADDR) 失败 " + lastSocketError());
        }

        sockaddr_in http_addr;
        std::memset(&http_addr, 0, sizeof(http_addr));
        http_addr.sin_family = AF_INET;
        http_addr.sin_port = htons(http_port);
        if (host.empty() || host == "0.0.0.0") http_addr.sin_addr.s_addr = INADDR_ANY;
        else http_addr.sin_addr.s_addr = inet_addr(host.c_str());

        if (bind(http_server_socket, (sockaddr*)&http_addr, sizeof(http_addr)) == SOCKET_ERROR_MSG) {
            log_error("绑定 HTTP 端口 " + std::to_string(http_port) + " 失败 " + lastSocketError());
            CLOSE_SOCKET(http_server_socket);
            CLOSE_SOCKET(tcp_server_socket);
            http_server_socket = INVALID_SOCKET_VALUE;
            tcp_server_socket = INVALID_SOCKET_VALUE;
            return false;
        }
        if (listen(http_server_socket, 5) == SOCKET_ERROR_MSG) {
            log_error("HTTP listen 失败 " + lastSocketError());
            CLOSE_SOCKET(http_server_socket);
            CLOSE_SOCKET(tcp_server_socket);
            http_server_socket = INVALID_SOCKET_VALUE;
            tcp_server_socket = INVALID_SOCKET_VALUE;
            return false;
        }

        running = true;
        stopping = false;

        tcp_accept_thread = std::thread(&TCP_HTTP_Server::accept_tcp_connections, this);
        http_accept_thread = std::thread(&TCP_HTTP_Server::accept_http_connections, this);

        // 服务器启动后，创建语音识别线程
        speech_thread_running_ = true;
        speech_recognition_thread_ = std::thread(&TCP_HTTP_Server::speech_recognition_worker, this);

        return true;
    } catch (const std::exception& e) {
        log_error(std::string("启动服务器失败：") + e.what());
        stop();
        return false;
    }
}

void TCP_HTTP_Server::stop() {
    if (!running.exchange(false)) return;
    stopping = true;

    if (tcp_server_socket != INVALID_SOCKET_VALUE) {
        CLOSE_SOCKET(tcp_server_socket);
        tcp_server_socket = INVALID_SOCKET_VALUE;
    }
    if (http_server_socket != INVALID_SOCKET_VALUE) {
        CLOSE_SOCKET(http_server_socket);
        http_server_socket = INVALID_SOCKET_VALUE;
    }

    if (tcp_accept_thread.joinable()) tcp_accept_thread.join();
    if (http_accept_thread.joinable()) http_accept_thread.join();

    // 唤醒所有等待客户端结果的任务线程，避免关闭时长时间阻塞
    {
        std::lock_guard<std::mutex> lock(pending_requests_mutex_);
        for (auto& kv : pending_requests_) {
            std::lock_guard<std::mutex> l(kv.second->mtx);
            kv.second->fulfilled = true;   // 结果留空，仅用于唤醒等待线程
        }
        pending_requests_.clear();
    }

    // 停止语音识别线程
    speech_thread_running_ = false;
    speech_task_cv_.notify_all();
    if (speech_recognition_thread_.joinable()) speech_recognition_thread_.join();

    {
        std::lock_guard<std::mutex> lock(handlers_mutex);
        for (auto& h : client_handlers) {
            if (h && h->socket_ != INVALID_SOCKET_VALUE) {
                CLOSE_SOCKET(h->socket_);
                h->socket_ = INVALID_SOCKET_VALUE;
            }
        }
    }

    {
        std::vector<std::shared_ptr<ClientHandler>> snapshot;
        {
            std::lock_guard<std::mutex> lock(handlers_mutex);
            snapshot.swap(client_handlers);
        }
        for (auto& h : snapshot) {
            if (h) h->join();
        }
    }

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.clear();
    }

    stopping = false;
}

void TCP_HTTP_Server::accept_tcp_connections() {
    while (running) {
        sockaddr_in addr;
        int addr_len = sizeof(addr);
        socket_t client = accept(tcp_server_socket, (sockaddr*)&addr, &addr_len);
        if (client == INVALID_SOCKET_VALUE) {
            if (!running) break;
            bool transient = false;
#if defined(WIN32) || defined(WIN64)
            int err = WSAGetLastError();
            if (err == WSAEINTR || err == WSAECONNRESET) transient = true;
#else
            if (errno == EINTR) transient = true;
#endif
            if (transient) continue;
            log_error("接受 TCP 连接时出错 " + lastSocketError());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            if (clients.size() >= 3)
            {
                if (AIAVIPCheck() != 0)
                {
                    log_warn("server 拒绝连接：客户端数已达3个且非年费会员，来自 " + addr_to_string(addr));
                    CLOSE_SOCKET(client);
                    continue;
                }
            }
        }
        auto handler = std::make_shared<ClientHandler>(client, addr, this);
        {
            std::lock_guard<std::mutex> lock(handlers_mutex);
            client_handlers.push_back(handler);
        }
        handler->start();
    }
}

void TCP_HTTP_Server::accept_http_connections() {
    while (running) {
        sockaddr_in addr;
        int addr_len = sizeof(addr);
        socket_t client = accept(http_server_socket, (sockaddr*)&addr, &addr_len);
        if (client == INVALID_SOCKET_VALUE) {
            if (!running) break;
            bool transient = false;
#if defined(WIN32) || defined(WIN64)
            int err = WSAGetLastError();
            if (err == WSAEINTR || err == WSAECONNRESET) transient = true;
#else
            if (errno == EINTR) transient = true;
#endif
            if (transient) continue;
            log_error("接受 HTTP 连接时出错 " + lastSocketError());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        std::thread([this, client, addr]() {
            handle_http_client(client, addr);
        }).detach();
    }
}

std::string TCP_HTTP_Server::url_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '+') {
            out.push_back(' ');
        } else if (c == '%' && i + 2 < s.size()) {
            auto hex_val = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                return -1;
            };
            int hi = hex_val(s[i + 1]);
            int lo = hex_val(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                out.push_back(c);
            }
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::map<std::string, std::string> TCP_HTTP_Server::parse_query_string(const std::string& qs) {
    std::map<std::string, std::string> out;
    size_t i = 0;
    while (i < qs.size()) {
        size_t amp = qs.find('&', i);
        std::string kv = (amp == std::string::npos) ? qs.substr(i) : qs.substr(i, amp - i);
        i = (amp == std::string::npos) ? qs.size() : amp + 1;
        size_t eq = kv.find('=');
        if (eq != std::string::npos) {
            out[url_decode(kv.substr(0, eq))] = url_decode(kv.substr(eq + 1));
        } else {
            out[url_decode(kv)] = "";
        }
    }
    return out;
}

void TCP_HTTP_Server::http_send_response(socket_t sock, int status_code,
                                          const std::string& content_type,
                                          const std::string& body) {
    std::string status_text;
    switch (status_code) {
        case 200: status_text = "OK"; break;
        case 400: status_text = "Bad Request"; break;
        case 404: status_text = "Not Found"; break;
        case 500: status_text = "Internal Server Error"; break;
        default:  status_text = "Unknown";
    }
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    std::string resp = oss.str() + body;
    const char* p = resp.c_str();
    int total = 0;
    int len = static_cast<int>(resp.size());
    while (total < len) {
        int n = send(sock, p + total, len - total, 0);
        if (n == SOCKET_ERROR_MSG || n <= 0) return;
        total += n;
    }
}

void TCP_HTTP_Server::http_send_json(socket_t sock, int status_code, const json& data) {
    std::string body = data.dump();
    http_send_response(sock, status_code, "application/json; charset=utf-8", body);
}

void TCP_HTTP_Server::http_send_page(socket_t sock) {
    const char* html =
"<!DOCTYPE html>\n"
"<html lang=\"zh-CN\">\n"
"<head>\n"
"    <meta charset=\"UTF-8\">\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"    <title>服务器管理面板</title>\n"
"    <style>\n"
"        * { margin: 0; padding: 0; box-sizing: border-box; }\n"
"        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; }\n"
"        .container { max-width: 800px; margin: 0 auto; background: white; border-radius: 10px; box-shadow: 0 10px 40px rgba(0,0,0,0.2); overflow: hidden; }\n"
"        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 20px 30px; text-align: center; }\n"
"        .header h1 { font-size: 24px; margin-bottom: 5px; }\n"
"        .header p { opacity: 0.9; font-size: 14px; }\n"
"        .content { padding: 30px; }\n"
"        .section { margin-bottom: 30px; }\n"
"        .section-title { font-size: 18px; color: #333; margin-bottom: 15px; padding-bottom: 10px; border-bottom: 2px solid #667eea; }\n"
"        .client-list { list-style: none; max-height: 300px; overflow-y: auto; border: 1px solid #e0e0e0; border-radius: 5px; }\n"
"        .client-item { padding: 12px 20px; border-bottom: 1px solid #f0f0f0; cursor: pointer; transition: all 0.3s; display: flex; align-items: center; }\n"
"        .client-item:hover { background: #f5f5f5; }\n"
"        .client-item.selected { background: #e8f0fe; border-left: 3px solid #667eea; }\n"
"        .client-item:last-child { border-bottom: none; }\n"
"        .client-icon { width: 30px; height: 30px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); border-radius: 50%; display: flex; align-items: center; justify-content: center; color: white; margin-right: 15px; font-weight: bold; }\n"
"        .client-name { flex: 1; font-weight: 500; }\n"
"        .client-status { width: 10px; height: 10px; background: #4caf50; border-radius: 50%; }\n"
"        .form-group { margin-bottom: 20px; }\n"
"        .form-group label { display: block; margin-bottom: 8px; color: #555; font-weight: 500; }\n"
"        .form-control { width: 100%; padding: 12px 15px; border: 1px solid #ddd; border-radius: 5px; font-size: 14px; transition: border-color 0.3s; }\n"
"        .form-control:focus { outline: none; border-color: #667eea; }\n"
"        textarea.form-control { min-height: 120px; resize: vertical; }\n"
"        .btn { padding: 12px 30px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; transition: transform 0.2s, box-shadow 0.2s; }\n"
"        .btn:hover { transform: translateY(-2px); box-shadow: 0 5px 20px rgba(102, 126, 234, 0.4); }\n"
"        .btn:active { transform: translateY(0); }\n"
"        .btn:disabled { background: #ccc; cursor: not-allowed; transform: none; box-shadow: none; }\n"
"        .message-box { padding: 15px; border-radius: 5px; margin-top: 15px; display: none; }\n"
"        .message-box.success { background: #d4edda; border: 1px solid #c3e6cb; color: #155724; }\n"
"        .message-box.error { background: #f8d7da; border: 1px solid #f5c6cb; color: #721c24; }\n"
"        .empty-state { text-align: center; padding: 40px 20px; color: #999; }\n"
"        .empty-state-icon { font-size: 48px; margin-bottom: 15px; }\n"
"        .refresh-btn { background: none; border: 1px solid #667eea; color: #667eea; padding: 8px 15px; border-radius: 5px; cursor: pointer; font-size: 14px; margin-left: 10px; }\n"
"        .refresh-btn:hover { background: #667eea; color: white; }\n"
"        .header-actions { display: flex; align-items: center; justify-content: center; margin-top: 10px; }\n"
"        .pwd-row { display: flex; gap: 10px; align-items: center; }\n"
"        .pwd-row .form-control { flex: 1; }\n"
"        .pwd-hint { font-size: 12px; color: #999; margin-top: 6px; }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <div class=\"container\">\n"
"        <div class=\"header\">\n"
"            <h1>AIA 服务器管理面板</h1>\n"
"            <div class=\"header-actions\">\n"
"                <p>在线用户及能力管理系统</p>\n"
"                <button class=\"refresh-btn\" onclick=\"loadClients()\">刷新</button>\n"
"            </div>\n"
"        </div>\n"
"        <div class=\"content\">\n"
"            <div class=\"section\" id=\"pwdSection\">\n"
"                <h2 class=\"section-title\">连接密码</h2>\n"
"                <div class=\"pwd-row\">\n"
"                    <input type=\"password\" id=\"serverPwd\" class=\"form-control\" placeholder=\"请输入服务器连接密码（如未设置可留空）\">\n"
"                    <button class=\"btn\" onclick=\"savePassword()\">确定</button>\n"
"                </div>\n"
"                <div class=\"pwd-hint\" id=\"pwdHint\">服务器设置了连接密码时需要输入，输入后点击确定加载客户端列表</div>\n"
"            </div>\n"
"            <div class=\"section\">\n"
"                <h2 class=\"section-title\">在线客户端列表</h2>\n"
"                <ul class=\"client-list\" id=\"clientList\"><li class=\"empty-state\"><div class=\"empty-state-icon\">加载中...</div></li></ul>\n"
"            </div>\n"
"            <div class=\"section\">\n"
"                <h2 class=\"section-title\">发送消息</h2>\n"
"                <div class=\"form-group\"><label for=\"selectedUser\">接收用户：</label><input type=\"text\" id=\"selectedUser\" class=\"form-control\" readonly placeholder=\"请先从列表选择用户\"></div>\n"
"                <div class=\"form-group\"><label for=\"messageContent\">消息内容：</label><textarea id=\"messageContent\" class=\"form-control\" placeholder=\"输入要发送的消息...\"></textarea></div>\n"
"                <button class=\"btn\" id=\"sendBtn\" onclick=\"sendMessage()\" disabled>发送消息</button>\n"
"                <div id=\"messageBox\" class=\"message-box\"></div>\n"
"            </div>\n"
"        </div>\n"
"    </div>\n"
"    <script>\n"
"        let selectedClient = null; let clients = [];\n"
"        let serverPassword = localStorage.getItem('aia_server_pwd') || '';\n"
"        document.getElementById('serverPwd').value = serverPassword;\n"
"\n"
"        // 所有请求统一带上 x-server-password 头\n"
"        function fetchWithAuth(url, options) {\n"
"            options = options || {};\n"
"            options.headers = options.headers || {};\n"
"            if (serverPassword) options.headers['x-server-password'] = serverPassword;\n"
"            return fetch(url, options);\n"
"        }\n"
"\n"
"        function savePassword() {\n"
"            serverPassword = document.getElementById('serverPwd').value;\n"
"            localStorage.setItem('aia_server_pwd', serverPassword);\n"
"            document.getElementById('pwdHint').textContent = serverPassword ? '密码已保存' : '未设置密码（适用于无密码服务器）';\n"
"            loadClients();\n"
"        }\n"
"\n"
"        function handleAuthError() {\n"
"            serverPassword = '';\n"
"            localStorage.removeItem('aia_server_pwd');\n"
"            document.getElementById('serverPwd').value = '';\n"
"            document.getElementById('pwdHint').textContent = '密码错误或服务器需要连接密码，请重新输入';\n"
"            showError('服务器需要连接密码，请在上方输入密码后点击确定');\n"
"            document.getElementById('serverPwd').focus();\n"
"        }\n"
"\n"
"        async function loadClients() {\n"
"            try {\n"
"                const r = await fetchWithAuth('/clients_detail');\n"
"                if (r.status === 401) { handleAuthError(); return; }\n"
"                const d = await r.json();\n"
"                if (d.success) { clients = d.clients || []; renderClientList(clients); }\n"
"                else showError('加载用户列表失败：' + (d.error || '未知错误'));\n"
"            } catch (e) { showError('连接服务器失败：' + e.message); }\n"
"        }\n"
"        function renderClientList(l) {\n"
"            const ul = document.getElementById('clientList');\n"
"            if (l.length === 0) { ul.innerHTML = '<li class=\"empty-state\"><div class=\"empty-state-icon\">暂无在线用户</div></li>'; return; }\n"
"            ul.innerHTML = l.map(function(n){ var tags = []; if(n.commands) tags.push('命令:'+n.commands); return '<li class=\"client-item ' + (selectedClient === n.name ? 'selected' : '') + '\" onclick=\"selectClient(\\'' + n.name + '\\')\"><div class=\"client-icon\">' + n.name.charAt(0).toUpperCase() + '</div><span class=\"client-name\">' + n.name + (tags.length? '<br><small style=\\'color:#666\\'>'+tags.join(' | ')+'</small>' : '') + '</span><div class=\"client-status\"></div></li>'; }).join('');\n"
"        }\n"
"        function selectClient(n) { selectedClient = n; document.getElementById('selectedUser').value = n; document.getElementById('sendBtn').disabled = false; document.querySelectorAll('.client-item').forEach(function(i){ i.classList.remove('selected'); if (i.querySelector('.client-name').textContent.indexOf(n) === 0) i.classList.add('selected'); }); document.getElementById('messageBox').style.display = 'none'; }\n"
"        async function sendMessage() {\n"
"            const u = document.getElementById('selectedUser').value; const c = document.getElementById('messageContent').value.trim();\n"
"            if (!u) { showMessage('请先选择用户', 'error'); return; } if (!c) { showMessage('消息内容不能为空', 'error'); return; }\n"
"            const b = document.getElementById('sendBtn'); b.disabled = true; b.textContent = '发送中...';\n"
"            try {\n"
"                const r = await fetchWithAuth('/send_message', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ username: u, message: c }) });\n"
"                if (r.status === 401) { handleAuthError(); showMessage('密码错误或服务器需要连接密码', 'error'); return; }\n"
"                const d = await r.json();\n"
"                if (d.success) { showMessage('消息已成功发送给 ' + u, 'success'); document.getElementById('messageContent').value = ''; }\n"
"                else showMessage('发送失败：' + (d.error || '未知错误'), 'error');\n"
"            } catch (e) { showMessage('发送失败：' + e.message, 'error'); } finally { b.disabled = false; b.textContent = '发送消息'; }\n"
"        }\n"
"        function showMessage(t, ty) { const m = document.getElementById('messageBox'); m.textContent = t; m.className = 'message-box ' + ty; m.style.display = 'block'; setTimeout(function(){ m.style.display = 'none'; }, 3000); }\n"
"        function showError(t) { const ul = document.getElementById('clientList'); ul.innerHTML = '<li class=\"empty-state\"><div class=\"empty-state-icon\"></div><div>' + t + '</div></li>'; }\n"
"        // 已保存密码则自动加载，否则等待用户输入\n"
"        if (serverPassword) loadClients(); else showError('请输入服务器连接密码后点击确定');\n"
"        setInterval(function(){ if (serverPassword) loadClients(); }, 60000);\n"
"    </script>\n"
"</body>\n"
"</html>";
    http_send_response(sock, 200, "text/html; charset=utf-8", html);
}

void TCP_HTTP_Server::handle_http_client(socket_t sock, const sockaddr_in& addr) {
    try {
        char buf[8192];
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) { CLOSE_SOCKET(sock); return; }
        buf[n] = 0;
        std::string raw(buf);

        size_t line_end = raw.find("\r\n");
        if (line_end == std::string::npos) { http_send_response(sock, 400, "text/plain", "Bad Request"); CLOSE_SOCKET(sock); return; }
        std::string first_line = raw.substr(0, line_end);
        size_t sp1 = first_line.find(' ');
        if (sp1 == std::string::npos) { http_send_response(sock, 400, "text/plain", "Bad Request"); CLOSE_SOCKET(sock); return; }
        size_t sp2 = first_line.find(' ', sp1 + 1);
        if (sp2 == std::string::npos) { http_send_response(sock, 400, "text/plain", "Bad Request"); CLOSE_SOCKET(sock); return; }
        std::string method = first_line.substr(0, sp1);
        std::string path_full = first_line.substr(sp1 + 1, sp2 - sp1 - 1);

        size_t qm = path_full.find('?');
        std::string path = (qm == std::string::npos) ? path_full : path_full.substr(0, qm);
        std::string query = (qm == std::string::npos) ? "" : path_full.substr(qm + 1);

        size_t head_end = raw.find("\r\n\r\n");
        std::string headers_str = (head_end == std::string::npos) ? "" : raw.substr(line_end + 2, head_end - line_end - 2);
        std::string body_str = (head_end == std::string::npos) ? "" : raw.substr(head_end + 4);

        std::map<std::string, std::string> headers;
        {
            size_t p = 0;
            while (p < headers_str.size()) {
                size_t nl = headers_str.find("\r\n", p);
                std::string hline = (nl == std::string::npos) ? headers_str.substr(p) : headers_str.substr(p, nl - p);
                p = (nl == std::string::npos) ? headers_str.size() : nl + 2;
                size_t col = hline.find(':');
                if (col != std::string::npos) {
                    std::string k = trim(hline.substr(0, col));
                    std::string v = trim(hline.substr(col + 1));
                    std::transform(k.begin(), k.end(), k.begin(), ::tolower);
                    headers[k] = v;
                }
            }
        }

        int content_length = 0;
        auto cl_it = headers.find("content-length");
        if (cl_it != headers.end()) {
            try { content_length = std::stoi(cl_it->second); } catch (...) {}
        }

        while (static_cast<int>(body_str.size()) < content_length) {
            char rbuf[4096];
            int rn = recv(sock, rbuf, sizeof(rbuf), 0);
            if (rn <= 0) break;
            body_str.append(rbuf, rn);
        }

        // 连接服务器密码校验
        if (path != "/" && !http_check_auth(query, headers, addr)) {
            json err = { {"success", false}, {"error", "密码错误"} };
            http_send_json(sock, 401, err);
            CLOSE_SOCKET(sock);
            return;
        }

        if (method == "POST" && path == "/send_message") {
            json data;
            try {
                data = json::parse(body_str);
            } catch (...) {
                json err = { {"success", false}, {"error", "JSON解析错误"} };
                http_send_json(sock, 400, err);
                CLOSE_SOCKET(sock);
                return;
            }
            std::string username  = data.value("username", "");
            std::string message_t  = data.value("message", "");
            // 去掉消息末尾的换行符（部分 HTTP 客户端会带 \r\n）
            while (!message_t.empty() &&
                   (message_t.back() == '\n' || message_t.back() == '\r')) {
                message_t.pop_back();
            }
            if (username.empty() || message_t.empty()) {
                json err = { {"success", false}, {"error", "缺少username或message参数"} };
                http_send_json(sock, 400, err);
                CLOSE_SOCKET(sock);
                return;
            }
            json d = { {"content", message_t} };
            Message pm(AIACommand::PRIVATE_MESSAGE, d, "http_client", username);
            bool success = send_message_to_client(username, pm);
            if (success) {
                json resp = { {"success", true}, {"message", "消息已发送给 " + username} };
                http_send_json(sock, 200, resp);
            } else {
                json resp = { {"success", false}, {"error", "用户 " + username + " 不存在或未连接"} };
                http_send_json(sock, 404, resp);
            }
        } else if (method == "GET" && path == "/") {
            http_send_page(sock);
        } else if (method == "GET" && path == "/health") {
            json resp = { {"success", true}, {"status", "running"}, {"clients", static_cast<int>(get_clients_list().size())} };
            http_send_json(sock, 200, resp);
        } else if (method == "GET" && path == "/clients") {
            std::vector<std::string> list = get_clients_list();
            json arr = json::array();
            for (const auto& nm : list) arr.push_back(nm);
            json resp = { {"success", true}, {"clients", arr}, {"count", static_cast<int>(list.size())} };
            http_send_json(sock, 200, resp);
        } else if (method == "GET" && path == "/clients_detail") {
            std::vector<ClientInfo> list = get_clients_detail();
            json arr = json::array();
            for (const auto& ci : list) {
                arr.push_back({
                    {"name", ci.name},
                    {"commands", ci.commands},
                    {"online", ci.online}
                });
            }
            json resp = { {"success", true}, {"clients", arr}, {"count", static_cast<int>(list.size())} };
            http_send_json(sock, 200, resp);
        } else if (method == "GET" && path == "/check_username") {
            auto qp = parse_query_string(query);
            std::string username;
            auto it = qp.find("username");
            if (it != qp.end()) username = it->second;
            if (username.empty()) {
                json err = { {"success", false}, {"error", "缺少 username 参数"} };
                http_send_json(sock, 400, err);
                CLOSE_SOCKET(sock);
                return;
            }
            std::vector<std::string> list = get_clients_list();
            bool exists = std::find(list.begin(), list.end(), username) != list.end();
            json resp = { {"success", true}, {"username", username}, {"exists", exists} };
            http_send_json(sock, 200, resp);
        } else {
            json err = { {"success", false}, {"error", "路径不存在"} };
            http_send_json(sock, 404, err);
        }
    } catch (const std::exception& e) {
        log_error(std::string("处理HTTP请求时出错: ") + e.what());
        json err = { {"success", false}, {"error", std::string("服务器错误: ") + e.what()} };
        http_send_json(sock, 500, err);
    } catch (...) {
        log_error("处理HTTP请求时发生未知错误");
    }
    CLOSE_SOCKET(sock);
}

bool TCP_HTTP_Server::register_client(std::shared_ptr<ClientHandler> handler, std::string& error_reason) {
    Message broadcast_msg;
    bool need_broadcast = false;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        // 客户端数 >=3 时检查 VIP
        if (clients.size() >= 3)
        {
            if (AIAVIPCheck() != 0)
            {
                error_reason = "客户端数已达上限（非年费会员最多3个）";
                log_warn("server 拒绝注册：客户端数已达3个且非年费会员，" + handler->name);
                return false;
            }
        }
        if (clients.find(handler->name) != clients.end()) {
            error_reason = "名称 " + handler->name + " 已存在";
            log_warn("客户端名称 " + handler->name + " 已存在，拒绝注册");
            return false;
        }
        clients[handler->name] = handler;
        std::ostringstream oss;
        oss << "客户端 " << handler->name << " 加入了系统";
        if (!handler->caps.executable_commands.empty()) {
            oss << "（可执行命令：";
            bool first = true;
            for (auto& c : handler->caps.executable_commands) {
                if (!first) oss << "、"; oss << c; first = false;
            }
            oss << "）";
        }
        json d = { {"content", oss.str()} };
        broadcast_msg = Message(AIACommand::BROADCAST, d, "server", "all_users");
    }
    if (need_broadcast) broadcast_message(broadcast_msg);
    return true;
}

void TCP_HTTP_Server::unregister_client(std::shared_ptr<ClientHandler> handler) {
    Message broadcast_msg;
    bool need_broadcast = false;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        if (!handler->name.empty()) {
            auto it = clients.find(handler->name);
            // 仅当 map 中保存的是同一个 handler 时才移除，避免同名客户端被拒后
            // 误删已注册的合法客户端（否则第三次同名连接会因 map 已被清空而注册成功）
            if (it != clients.end() && it->second.get() == handler.get()) {
                clients.erase(it);
                json d = { {"content", "客户端 " + handler->name + " 离开了系统"} };
                broadcast_msg = Message(AIACommand::BROADCAST, d, "server", "all_users");
            }
        }
    }
    if (need_broadcast) broadcast_message(broadcast_msg);
}

void TCP_HTTP_Server::detach_handler(std::shared_ptr<ClientHandler> handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex);
    for (auto it = client_handlers.begin(); it != client_handlers.end(); ++it) {
        if (it->get() == handler.get()) {
            client_handlers.erase(it);
            return;
        }
    }
}

std::vector<std::string> TCP_HTTP_Server::get_clients_list() {
    std::lock_guard<std::mutex> lock(clients_mutex);
    std::vector<std::string> list;
    for (const auto& kv : clients) list.push_back(kv.first);
    return list;
}

std::vector<ClientInfo> TCP_HTTP_Server::get_clients_detail() {
    std::lock_guard<std::mutex> lock(clients_mutex);
    std::vector<ClientInfo> list;
    for (const auto& kv : clients) {
        ClientInfo info;
        info.name     = kv.first;
        if (kv.second) {
            const auto& caps = kv.second->caps;
            std::ostringstream cs;
            bool first = true;
            for (auto& c : caps.executable_commands) {
                if (!first) cs << "、"; cs << c; first = false;
            }
            info.commands = cs.str();
        }
        info.online = true;
        list.push_back(std::move(info));
    }
    return list;
}

void TCP_HTTP_Server::set_connect_password(const std::string& pwd) {
    std::lock_guard<std::mutex> lock(connect_pwd_mutex_);
    connect_password_ = pwd;
}

bool TCP_HTTP_Server::check_connect_password(const std::string& provided) {
    std::lock_guard<std::mutex> lock(connect_pwd_mutex_);
    if (connect_password_.empty()) return true;
    return provided == connect_password_;
}

bool TCP_HTTP_Server::http_check_auth(const std::string& query,
                                       const std::map<std::string, std::string>& headers,
                                       const sockaddr_in& addr) {
    {
        std::lock_guard<std::mutex> lock(connect_pwd_mutex_);
        if (connect_password_.empty()) return true;
    }
    if (ntohl(addr.sin_addr.s_addr) == INADDR_LOOPBACK) return true;

    std::string provided;
    auto qp = parse_query_string(query);
    auto it = qp.find("password");
    if (it != qp.end()) provided = it->second;
    if (provided.empty()) {
        auto hit = headers.find("x-server-password");
        if (hit != headers.end()) provided = hit->second;
    }
    return check_connect_password(provided);
}

bool TCP_HTTP_Server::send_message_to_client(const std::string& target_name, const Message& msg) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = clients.find(target_name);
    if (it != clients.end() && it->second) {
        return it->second->send_message(msg);
    }
    return false;
}

void TCP_HTTP_Server::broadcast_message(const Message& msg, const std::string& exclude) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& kv : clients) {
        if (kv.first != exclude && kv.second) {
            kv.second->send_message(msg);
        }
    }
}

// ============ AIA 能力路由 ============
std::string TCP_HTTP_Server::find_client_by_command(const std::string& cmd) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& kv : clients) {
        if (kv.second && kv.second->caps.hasCommand(cmd)) {
            return kv.first;
        }
    }
    return std::string();
}

// ============ 录制音频：把音频及客户端名称加入语音识别线程队列 ============
void TCP_HTTP_Server::handle_record_audio(const std::string& sender_name, const json& data) {
    // 多个麦克风设备客户端，发来音频的话，放在队列里，然后语音识别线程会读取队列依次识别
    SpeechTask task;
    task.client_name = sender_name;
    task.audio_info = data;   // 保留 音频格式/音频路径/音频数据
    {
        std::lock_guard<std::mutex> lock(speech_task_mutex_);
        speech_task_queue_.push(std::move(task));
    }
    speech_task_cv_.notify_one();
    log_info("[" + sender_name + "] 的音频已加入语音识别队列");
}

// ============ 语音识别线程主函数 ============
// 发现队列有新数据则取出，遍历所有客户端找到支持[语音识别]的客户端，
// 把音频发给该客户端识别；超时1分钟未返回则打印客户端出问题了，否则打印识别结果
void TCP_HTTP_Server::speech_recognition_worker() {
    while (speech_thread_running_) {
        SpeechTask task;
        {
            std::unique_lock<std::mutex> lock(speech_task_mutex_);
            speech_task_cv_.wait(lock, [&] {
                return !speech_task_queue_.empty() || !speech_thread_running_.load();
            });
            if (!speech_thread_running_) break;
            task = std::move(speech_task_queue_.front());
            speech_task_queue_.pop();
        }

        log_info("语音识别线程：取出 [" + task.client_name + "] 的音频");

        // 遍历所有客户端，查找支持[语音识别]的客户端（优先使用音频来源客户端）
        std::string target;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            auto it = clients.find(task.client_name);
            if (it != clients.end() && it->second &&
                it->second->caps.hasCommand(ExecutableCommand::SPEECH_RECOGNITION)) {
                target = task.client_name;
            }
            if (target.empty()) {
                for (const auto& kv : clients) {
                    if (kv.second && kv.second->caps.hasCommand(ExecutableCommand::SPEECH_RECOGNITION)) {
                        target = kv.first;
                        break;
                    }
                }
            }
        }
        if (target.empty()) {
            log_warn("没有找到支持[语音识别]的客户端，[" + task.client_name + "] 的音频丢弃");
            continue;
        }

        // 组装 [语音识别] 命令（带音频来源与音频信息）
        json req;
        req[FieldName::Name] = task.client_name;
        req[FieldName::AudioFormat] = task.audio_info.value(FieldName::AudioFormat, std::string());
        if (task.audio_info.contains(FieldName::AudioData)) {
            req[FieldName::AudioData] = task.audio_info[FieldName::AudioData];
        } else {
            std::string audio_path = task.audio_info.value(FieldName::AudioPath, std::string());
            if (!audio_path.empty()) req[FieldName::AudioPath] = audio_path;
        }

        // 发送给识别客户端并阻塞等待结果，超时1分钟
        Message msg(ExecutableCommand::SPEECH_RECOGNITION, req, "server", target);
        log_info("语音识别线程：向 [" + target + "] 发送 [" + task.client_name + "] 的音频进行识别");
        json result;
        if (send_and_wait(target, msg, result, 60)) {
            std::string text = result.value(FieldName::RecognitionResult, std::string());
            if (text.empty()) {
                log_warn("[" + target + "] 返回的语音识别结果为空");
            } else {
                log_info("[" + target + "] 语音识别结果: " + text);
                json chain = result;
                if (!chain.contains(FieldName::Name)) chain[FieldName::Name] = task.client_name;
                handle_speech_recognition_result(target, chain);
            }
        } else {
            log_error("客户端 [" + target + "] 出问题了（1分钟内未返回语音识别结果）");
        }
    }
    log_info("语音识别线程退出");
}

// ============ 录制视频：转发给支持[人脸识别]的客户端 ============
void TCP_HTTP_Server::handle_record_video(const std::string& sender_name, const json& data) {
    std::string video_format = data.value(FieldName::VideoFormat, std::string());
    std::string video_path   = data.value(FieldName::VideoPath, std::string());
    bool hasVideoData = data.contains(FieldName::VideoData);

    std::string target = find_client_by_command(ExecutableCommand::FACE_RECOGNITION);
    if (target.empty()) {
        log_warn("没有找到支持[人脸识别]的客户端，[录制视频]丢弃");
        return;
    }
    json req;
    req[FieldName::Name] = sender_name;
    req[FieldName::VideoFormat] = video_format;
    if (hasVideoData) {
        req[FieldName::VideoData] = data[FieldName::VideoData];
    } else if (!video_path.empty()) {
        req[FieldName::VideoPath] = video_path;
    }
    Message msg(ExecutableCommand::FACE_RECOGNITION, req, "server", target);
    send_message_to_client(target, msg);
}

// ============ 下发 [显示文本] 给所有支持该命令的客户端 ============
void TCP_HTTP_Server::send_text_display(const std::string& text) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& kv : clients) {
        if (kv.second && kv.second->caps.hasCommand(ExecutableCommand::TEXT_DISPLAY)) {
            json req;
            req[FieldName::Text] = text;
            Message tm(ExecutableCommand::TEXT_DISPLAY, req, "server", kv.first);
            kv.second->send_message(tm);
        }
    }
}

// ============ 语音识别结果：result 包含任意客户端提供的命令则启动任务线程 ============
void TCP_HTTP_Server::handle_speech_recognition_result(const std::string& sender_name, const json& data) {
    std::string origin_client = data.value(FieldName::Name, std::string());
    std::string result = data.value(FieldName::RecognitionResult, std::string());
    
    if (result.empty()) return;
    
    std::string text = origin_client + "收到" + result;
    send_text_display(text);
    
    if (!find_client_by_command(ExecutableCommand::PLAY_AUDIO).empty())
    {
        std::string tts_client = find_client_by_command(ExecutableCommand::SPEECH_GENERATION);
        if (!tts_client.empty())
        {
            json req;
            req[FieldName::Text] = text;
            Message msg(ExecutableCommand::SPEECH_GENERATION, req, "server", tts_client);
            
            json result_audio;
            if (send_and_wait(tts_client, msg, result_audio, 60)) {
                send_audio_play(result_audio);
            }
        }
    }

    // 收集所有在线客户端上报的命令
    std::set<std::string> all_cmds;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (auto& kv : clients) {
            if (kv.second) {
                all_cmds.insert(kv.second->caps.executable_commands.begin(),
                                kv.second->caps.executable_commands.end());
            }
        }
    }

    // result 包含任意客户端提供的命令，则启用任务
    bool has_commands = false;
    for (const auto& cmd : all_cmds) {
        if (!cmd.empty() && result.find(cmd) != std::string::npos) { has_commands = true; break; }
    }

    if (has_commands) {
        // 文本包含客户端命令，启动任务线程处理
        //log_info("任务线程启动：origin=" + origin_client + " text=" + result);
        //std::thread(&TCP_HTTP_Server::run_task, this, origin_client, result).detach();
    }
}

// ============ 人脸识别结果：显示文本 + 触发语音生成 ============
void TCP_HTTP_Server::handle_face_recognition_result(const std::string& sender_name, const json& data) {
    std::string origin_client = data.value(FieldName::Name, std::string());
    std::string result = data.value(FieldName::RecognitionResult, std::string());

    std::string text = "[" + origin_client + "] 看到 " + result;
    send_text_display(text);

    if (find_client_by_command(ExecutableCommand::PLAY_AUDIO).empty()) return;
    std::string tts_client = find_client_by_command(ExecutableCommand::SPEECH_GENERATION);
    if (tts_client.empty()) return;
    json req;
    req[FieldName::Text] = text;
    Message msg(ExecutableCommand::SPEECH_GENERATION, req, "server", tts_client);
    send_message_to_client(tts_client, msg);
}

void TCP_HTTP_Server::send_audio_play(const json& data) {
    std::string audio_format = data.value(FieldName::AudioFormat, std::string());
    std::string audio_path   = data.value(FieldName::AudioPath, std::string());
    bool hasAudioData = data.contains(FieldName::AudioData);

    // 把生成的语音下发给所有支持[播放音频]的客户端
    json payload;
    payload[FieldName::AudioFormat] = audio_format;
    if (hasAudioData) {
        payload[FieldName::AudioData] = data[FieldName::AudioData];
    } else if (!audio_path.empty()) {
        payload[FieldName::AudioPath] = audio_path;
    }
    
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& kv : clients) {
        if (kv.second && kv.second->caps.hasCommand(ExecutableCommand::PLAY_AUDIO)) {
            Message play(ExecutableCommand::PLAY_AUDIO, payload, "server", kv.first);
            kv.second->send_message(play);
        }
    }
}

// ============ 命令解析规划任务（暂空） ============
void TCP_HTTP_Server::plan_command_task(const std::string& origin_client, const std::string& text) {
    (void)origin_client; (void)text;
}

// ============ 任务线程机制：发送命令并等待结果 ============
// 每条命令生成唯一请求ID随消息下发，客户端回传结果时原样带回该ID，
// 服务器按ID精确匹配等待者——多个任务线程并发向同一客户端请求同一命令也不会串结果
bool TCP_HTTP_Server::send_and_wait(const std::string& target, const Message& msg,
                                     json& out_result, int timeout_seconds) {
    auto pending = std::make_shared<PendingRequest>();
    uint64_t req_id = ++next_request_id_;   // 原子递增生成唯一请求ID

    {
        std::lock_guard<std::mutex> lock(pending_requests_mutex_);
        pending_requests_[req_id] = pending;
    }

    // 复制消息并带上请求ID下发给客户端
    Message out = msg;
    out.id = req_id;
    printf("id send:%lld\n", req_id);

    if (!send_message_to_client(target, out)) {
        std::lock_guard<std::mutex> lock(pending_requests_mutex_);
        pending_requests_.erase(req_id);
        return false;
    }

    std::unique_lock<std::mutex> lock(pending->mtx);
    bool ok = pending->cv.wait_for(lock, std::chrono::seconds(timeout_seconds),
                                    [&] { return pending->fulfilled; });

    {
        std::lock_guard<std::mutex> lock2(pending_requests_mutex_);
        pending_requests_.erase(req_id);
    }

    // 服务器关闭时被唤醒（结果为空）：按失败处理
    if (!running.load()) return false;

    if (ok) {
        out_result = std::move(pending->result);
        return true;
    }
    return false; // 超时
}

// ============ 检查是否有任务线程在等待此结果 ============
// 客户端必须原样回传命令中的请求ID，服务器按ID精确匹配等待的任务线程；
// 未回传ID（id=0）或ID找不到等待者，则视为无任务线程在等待
bool TCP_HTTP_Server::try_fulfill_pending(uint64_t request_id, const json& data) {
    if (request_id == 0) return false;
    std::shared_ptr<PendingRequest> pending;
    {
        std::lock_guard<std::mutex> lock(pending_requests_mutex_);
        auto it = pending_requests_.find(request_id);
        if (it == pending_requests_.end()) return false;
        pending = it->second;
        pending_requests_.erase(it);
    }
    {
        std::lock_guard<std::mutex> l(pending->mtx);
        pending->result = data;
        pending->fulfilled = true;
    }
    pending->cv.notify_one();
    return true;
}

// ============ 任务线程主函数：用 result 查找第一个支持其中命令的客户端，下发命令并等待打印结果 ============
void TCP_HTTP_Server::run_task(const std::string& origin_client, const std::string& text) {
    (void)origin_client;

    // 遍历所有客户端，找到第一个其命令出现在 result 中的客户端
    std::string target;
    std::string cmd;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (auto& kv : clients) {
            if (!kv.second) continue;
            for (const auto& c : kv.second->caps.executable_commands) {
                if (!c.empty() && text.find(c) != std::string::npos) {
                    target = kv.first;
                    cmd = c;
                    break;
                }
            }
            if (!target.empty()) break;
        }
    }

    if (target.empty()) {
        log_warn("任务：result 中没有找到客户端支持的命令，任务结束");
        return;
    }

    // 给该客户端发送对应命令，消息带上 result
    json req;
    req[FieldName::Text] = text;
    Message msg(cmd, req, "server", target);
    log_info("任务：向 [" + target + "] 发送 [" + cmd + "]，内容: " + text);

    // 等待客户端返回后打印结果
    json result_data;
    if (send_and_wait(target, msg, result_data, 1800)) {
        std::string shown = result_data.value(FieldName::RecognitionResult, std::string());
        if (shown.empty()) shown = result_data.value(FieldName::Text, std::string());
        if (shown.empty()) shown = result_data.dump();
        log_info("任务：[" + target + "] 返回 [" + cmd + "] 结果: " + shown);
    } else {
        log_warn("任务：等待 [" + target + "] 的 [" + cmd + "] 结果超时，任务终止");
    }
}

// ==================== 全局函数 & main ====================
void server_start() {
    if (g_server) return;
    std::string host = "0.0.0.0";
    int tcp_port = 8180;
    int http_port = 8181;

    g_server = new TCP_HTTP_Server(host, tcp_port, http_port);
    g_server->start();
}

void server_stop() {
    if (g_server) {
        g_server->stop();
        delete g_server;
        g_server = nullptr;
    }
}

TCP_HTTP_Server* get_server_instance() {
    return g_server;
}
