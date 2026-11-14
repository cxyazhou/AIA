#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <memory>
#include <map>
#include <atomic>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include <set>
#include <queue>
#include <condition_variable>

#ifndef AIA_PROTOCOL_COMMON_H
#define AIA_PROTOCOL_COMMON_H

#if defined(WIN32) || defined(WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSE_SOCKET closesocket
#define SOCKET_ERROR_MSG SOCKET_ERROR
using socket_t = SOCKET;
constexpr socket_t INVALID_SOCKET_VALUE = static_cast<socket_t>(INVALID_SOCKET);
#else
#include <sys/socket.h>
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
constexpr socket_t INVALID_SOCKET_VALUE = -1;
#endif

// 获取当前 socket 错误码及描述（跨平台，用于日志输出）
inline std::string lastSocketError() {
#if defined(WIN32) || defined(WIN64)
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

// ========== 可执行命令（客户端能力与命令的唯一标识，客户端上报/服务器下发/结果回传均使用同名命令） ==========
namespace ExecutableCommand {
    static constexpr const char* RECORD_AUDIO       = "录制音频";
    static constexpr const char* RECORD_VIDEO       = "录制视频";
    static constexpr const char* SPEECH_RECOGNITION = "语音识别";
    static constexpr const char* FACE_RECOGNITION   = "人脸识别";
    static constexpr const char* SPEECH_GENERATION  = "语音生成";
    static constexpr const char* PLAY_AUDIO         = "播放音频";
    static constexpr const char* PLAY_VIDEO         = "播放视频";
    static constexpr const char* TEXT_DISPLAY       = "显示文本";
}

// ========== 基础协议消息类型 ==========
namespace AIACommand {
    static constexpr const char* REGISTER          = "register";
    static constexpr const char* GET_CLIENTS       = "get_clients";
    static constexpr const char* SEND_MESSAGE      = "send_message";
    static constexpr const char* BROADCAST         = "broadcast";
    static constexpr const char* PRIVATE_MESSAGE   = "private";
    static constexpr const char* HEARTBEAT         = "heartbeat";
    static constexpr const char* RESPONSE          = "response";
}

// ========== 字段名 ==========
namespace FieldName {
    static constexpr const char* Name              = "客户端名称";
    static constexpr const char* AudioFormat       = "音频格式";
    static constexpr const char* AudioPath         = "音频路径";
    static constexpr const char* AudioData         = "音频数据";   // base64 string 或 bytes(array)
    static constexpr const char* VideoFormat       = "视频格式";
    static constexpr const char* VideoPath         = "视频路径";
    static constexpr const char* VideoData         = "视频数据";
    static constexpr const char* RecognitionResult = "识别结果";
    static constexpr const char* Text              = "文本";
}

// 消息类
class Message {
public:
    uint64_t id;
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

#endif // AIA_PROTOCOL_COMMON_H

// ==================== 前置声明 ====================
class TCP_HTTP_Server;

// ==================== 任务线程等待请求（任务线程发送命令后阻塞等待客户端回传结果） ====================
struct PendingRequest {
    json result;
    bool fulfilled = false;
    std::condition_variable cv;
    std::mutex mtx;
};

// ==================== 语音识别任务（录制音频入队，由语音识别线程处理） ====================
struct SpeechTask {
    std::string client_name;   // 音频来源客户端名称
    json        audio_info;    // 音频信息（音频格式/音频路径/音频数据）
};

// ==================== 客户端能力（服务器内存中保存完整能力） ====================
struct ClientCapabilities {
    std::set<std::string> executable_commands;  // 可执行命令：录制音频/录制视频/播放音频/...
    bool hasCommand(const std::string& c) const {
        return executable_commands.count(c) > 0;
    }
};

struct ClientInfo {
    std::string name;
    std::string commands;     // 可读的可执行命令
    bool        online;
};

// ==================== 客户端处理器 ====================
class ClientHandler : public std::enable_shared_from_this<ClientHandler> {
public:
    socket_t socket_;
    sockaddr_in address_;
    TCP_HTTP_Server* server_;
    std::string name;
    ClientCapabilities caps;          // 客户端上报的能力
    std::atomic<bool> running;
    std::atomic<bool> closed_;
    std::atomic<bool> joined_;
    std::string buffer;
    std::thread thread_;

    ClientHandler(socket_t sock, const sockaddr_in& addr, TCP_HTTP_Server* srv);
    ~ClientHandler();

    void start();
    void handle_client();
    void process_message(const Message& msg);
    void send_response(bool success, const std::string& message, const json& data = json::object());
    bool send_message(const Message& msg);
    void close();
    bool joinable() const { return thread_.joinable() && !joined_.load(std::memory_order_acquire); }
    void join() {
        bool expected = false;
        if (!joined_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
        if (thread_.joinable()) thread_.join();
    }
};

// ==================== TCP + HTTP 服务器 ====================
class TCP_HTTP_Server {
public:
    std::string host;
    int tcp_port;
    int http_port;

    std::unordered_map<std::string, std::shared_ptr<ClientHandler>> clients;
    std::mutex clients_mutex;
    std::atomic<bool> running;
    std::atomic<bool> stopping;

    socket_t tcp_server_socket;
    socket_t http_server_socket;
    std::thread tcp_accept_thread;
    std::thread http_accept_thread;
    std::thread speech_recognition_thread_;   // 语音识别线程（处理录制音频的识别）

    std::vector<std::shared_ptr<ClientHandler>> client_handlers;
    std::mutex handlers_mutex;

    TCP_HTTP_Server(const std::string& h = "0.0.0.0", int tcp_p = 8080, int http_p = 8081);
    ~TCP_HTTP_Server();

    bool start();
    void stop();
    void accept_tcp_connections();
    void accept_http_connections();
    void handle_http_client(socket_t client_sock, const sockaddr_in& addr);

    bool register_client(std::shared_ptr<ClientHandler> handler, std::string& error_reason);
    void unregister_client(std::shared_ptr<ClientHandler> handler);
    void detach_handler(std::shared_ptr<ClientHandler> handler);
    std::vector<std::string> get_clients_list();
    std::vector<ClientInfo>  get_clients_detail();
    bool send_message_to_client(const std::string& target_name, const Message& msg);
    void broadcast_message(const Message& msg, const std::string& exclude = "");

    // 连接服务器密码
    void set_connect_password(const std::string& pwd);
    bool check_connect_password(const std::string& provided);
    bool http_check_auth(const std::string& query,
                         const std::map<std::string, std::string>& headers,
                         const sockaddr_in& addr);

    // ============ 命令路由 ============
    // 按命令选择第一个支持的客户端（返回其 name；空字符串表示未找到）
    std::string find_client_by_command(const std::string& cmd);

    // [录制音频] -> 找支持[语音识别]的客户端，下发 [语音识别]（消息带音频信息）
    void handle_record_audio(const std::string& sender_name, const json& data);
    // [录制视频] -> 找支持[人脸识别]的客户端，下发 [人脸识别]（消息带视频信息）
    void handle_record_video(const std::string& sender_name, const json& data);
    // [语音识别]结果 -> 下发 [显示文本]（"[来源客户端] 听到 xxx"）；
    //                   若存在支持[播放音频]的客户端，再找支持[语音生成]的客户端下发 [语音生成]
    void handle_speech_recognition_result(const std::string& sender_name, const json& data);
    // [人脸识别]结果 -> 同语音识别结果链路，文本为 "[来源客户端] 看到 xxx"
    void handle_face_recognition_result(const std::string& sender_name, const json& data);
    // [语音生成]结果 -> 打印 "[客户端] 生成语音"，并把生成的语音以 [播放音频] 下发给所有支持播放音频的客户端
    void send_audio_play(const json& data);

    // 下发 [显示文本] 给所有支持该命令的客户端
    void send_text_display(const std::string& text);

    // 命令解析规划任务（暂时为空，后续扩展）
    void plan_command_task(const std::string& origin_client, const std::string& text);

    // ============ 任务线程机制 ============
    // 任务线程发送命令给客户端并阻塞等待结果（超时 30 分钟）
    bool send_and_wait(const std::string& target, const Message& msg,
                       json& out_result, int timeout_seconds = 1800);
    // 在 process_message 中调用：客户端必须原样回传请求ID，按ID精确匹配等待的任务线程
    bool try_fulfill_pending(uint64_t request_id, const json& data);
    // 任务线程主函数：用 result 查找第一个支持其中命令的客户端，下发命令（消息带 result）并等待打印结果
    void run_task(const std::string& origin_client, const std::string& text);

    // ============ 语音识别线程 ============
    // 语音识别线程主函数：从队列取音频，遍历客户端找到支持[语音识别]的客户端下发识别（超时1分钟）
    void speech_recognition_worker();

private:
    std::string      connect_password_;
    std::mutex       connect_pwd_mutex_;

    // 任务线程等待表：key = 请求ID（消息中的 id 字段），客户端回传结果时按ID精确匹配
    std::map<uint64_t, std::shared_ptr<PendingRequest>> pending_requests_;
    std::mutex      pending_requests_mutex_;
    std::atomic<uint64_t> next_request_id_{0};   // 请求ID生成器（原子递增）

    // 语音识别任务队列（录制音频 -> 语音识别线程）
    std::queue<SpeechTask>  speech_task_queue_;
    std::mutex              speech_task_mutex_;
    std::condition_variable speech_task_cv_;
    std::atomic<bool>       speech_thread_running_;

    void http_send_response(socket_t sock, int status_code, const std::string& content_type,
                            const std::string& body);
    void http_send_json(socket_t sock, int status_code, const json& data);
    void http_send_page(socket_t sock);
    std::map<std::string, std::string> parse_query_string(const std::string& qs);
    static std::string url_decode(const std::string& s);
};

// ==================== 全局函数 ====================
void server_start();
void server_stop();
TCP_HTTP_Server* get_server_instance();

#endif // SERVER_H
