#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include <set>

// Server.h 与 TcpClient.h 存在公共定义，统一 guard 防止同时包含时重复定义
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
    // 客户端主动上报给服务器的命令
    static constexpr const char* RECORD_AUDIO       = "录制音频";
    static constexpr const char* RECORD_VIDEO       = "录制视频";
    // 服务器下发给客户端执行的命令，客户端执行完成后以同名命令回传结果
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
    static constexpr const char* AudioData         = "音频数据";   // base64 string
    static constexpr const char* VideoFormat       = "视频格式";
    static constexpr const char* VideoPath         = "视频路径";
    static constexpr const char* VideoData         = "视频数据";   // base64 string
    static constexpr const char* RecognitionResult = "识别结果";
    static constexpr const char* Text              = "文本";
}

// 消息：JSON 行协议的基本单元，一行一条
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

// 客户端命令配置（注册时上报给服务器）
struct ClientCapabilityConfig {
    std::set<std::string> executable_commands;  // 支持的命令：录制音频/录制视频/播放音频/播放视频/显示文本/语音识别/语音生成/人脸识别

    void addCommand(const std::string& c) { executable_commands.insert(c); }
};

// 收到服务器下发的命令时的回调：上层在此实现具体命令。
//   commandType: 服务器下发的命令（"语音识别" "人脸识别" "语音生成" "显示文本" "播放音频" "播放视频"）
//   data:        命令参数
//   context:     透传指针（setCommandHandler 时传入）
// 返回 true 表示命令已被上层消费；返回 false 则走 TCPClient 的默认处理（测试实现）。
using AIACommandHandler = std::function<bool(
    const std::string& commandType,
    const json& data,
    void* context
)>;

// TCP 客户端：连接 AIA 服务器，注册命令能力，收发消息与命令
class TCPClient {
public:
    TCPClient(const std::string& host = "127.0.0.1", int port = 8180);
    ~TCPClient();

    // 连接并注册（上报支持的命令）
    bool connectWithCapabilities(const std::string& name,
                                 const ClientCapabilityConfig& caps,
                                 const std::string& password = "");

    // 设置命令处理回调
    void setCommandHandler(AIACommandHandler handler, void* context = nullptr);

    // ========== 命令执行结果回传（以同名命令回传给服务器） ==========
    bool sendSpeechGenerationResult(uint64_t id, const std::string& text,
                                    const std::string& format,
                                    const std::string& path,
                                    const char* data, int len);

    // ========== 消息 ==========
    void getClients();
    bool sendPrivateMessage(const std::string& target, const std::string& content);
    bool broadcast(const std::string& content);

    void closeConnect();
    bool isRunning() const { return m_running.load(); }
    bool isRegistered() const { return m_registered.load(); }
    std::string getLastError() const { return m_lastError; }
    const ClientCapabilityConfig& capabilities() const { return m_caps; }
    const std::string& name() const { return m_name; }

private:
    bool register_();
    bool sendMessage(const Message& msg);
    bool establishSocket();   // 创建 socket 并建立 TCP 连接（不启动线程、不注册）
    void receiveMessages();
    void handleMessage(const Message& msg);
    void updateClientsList(const json& clients);
    void heartbeatLoop();

    // 默认命令处理（上层回调未消费时走这里，回传测试数据）
    void defaultHandleSpeechGeneration(uint64_t id, const json& data);

private:
    std::string m_host;
    int m_port;
    socket_t m_socket;
    std::string m_name;
    std::string m_password;
    ClientCapabilityConfig m_caps;   // 本客户端上报的命令能力
    std::string m_lastError;
    std::atomic<bool> m_running;
    std::atomic<bool> m_registered;
    std::atomic<bool> m_userClose;   // 用户主动关闭，置位后接收线程不再自动重连
    std::thread m_recvThread;
    std::thread m_heartbeatThread;
    std::mutex m_mutex;
    std::mutex m_sendMutex;
    std::vector<std::string> m_clientsList;

    AIACommandHandler m_cmdHandler;
    void* m_cmdContext;
};

// ========== 辅助函数 ==========
// 解析 "host:port" / "tcp://host:port"，端口缺省 8180；失败返回 {"", -1}
std::pair<std::string, int> parseServerAddress(const std::string& addrStr);
// 域名/IP 解析为 sockaddr_in
bool resolveHost(const std::string& host, int port, sockaddr_in& out);

// ========== 全局单例接口 ==========
// 创建并连接客户端实例（已存在则直接返回）。
//   commandsStr: 支持的命令列表，以 "、" 或 "," 分隔，例如 "录制音频、录制视频、显示文本"
TCPClient* TcpClientInstance(const char* address, const char* name,
                             const char* password = nullptr,
                             const char* commandsStr = nullptr);
void TcpClientCloseInstance();
TCPClient* TcpClientGet();

// 发送消息：name 为空时广播，否则私聊
void TcpClientSendMsg(const char* name, const char* msg);

#endif // TCP_CLIENT_H
