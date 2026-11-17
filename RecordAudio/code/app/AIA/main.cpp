#if defined(WIN32) || defined(WIN64)
#pragma comment(linker,"/subsystem:windows /entry:mainCRTStartup")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "legacy_stdio_definitions.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "Normaliz.lib")

#pragma comment(lib, "RecordAudio.lib")
#pragma comment(lib, "AudioDevice.lib")
#pragma comment(lib, "TcpClient.lib")
#pragma comment(lib, "CPPBase.lib")
#pragma comment(lib, "GlobalShortCut.lib")

#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "libssl.lib")

#pragma comment(lib, "portaudio.lib")

#endif //defined(WIN32) || defined(WIN64)

#include "RecordAudio.h"
#include "AudioDevice.h"
#include "GlobalShortCut.h"
#include "TcpClient.h"
#include "CPPBase.h"

#include <string>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <cctype>

// 客户端配置（名称 / 服务器地址 / 连接密码），从 exe 同目录 Config/client.cfg 读取
static std::string g_client_name = "麦克风";
static std::string g_server_address = "127.0.0.1";
static std::string g_server_pwd = "MyAIAServer";

// 去除首尾空白
static std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// 从 <exe目录>/client.cfg 读取配置，文件不存在或某字段缺失时使用默认值。
// 文件格式：每行 key=value，# 开头为注释。
static void loadConfigFromFile()
{
    char path[1024] = { 0 };
    char processname[1024] = { 0 };
    ICPPFile::GetExecutablePath(path, processname, sizeof(path));

    std::string cfgPath = std::string(path) + "client.cfg";
    std::ifstream ifs(cfgPath);
    if (!ifs.is_open()) {
        std::cerr << "未找到配置文件 " << cfgPath << "，使用默认配置" << std::endl;
        return;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';') continue;
        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq + 1));
        if (key == "name")     g_client_name = val;
        else if (key == "address") g_server_address = val;
        else if (key == "password") g_server_pwd = val;
    }
}

void Key_Space_Press_Callback()
{
    CPPRecordAudio::instance()->StartSpeak();
}

void Key_Space_Release_Callback()
{
    CPPRecordAudio::instance()->StopSpeak();
}

class ModuleInit : public CCPPThread
{
public:    
	ModuleInit()
    :CCPPThread("ModuleInitThread", 0, 10)
    {
        ThreadStart();
    };
    
	~ModuleInit()
    {
        CPPRecordAudio::instance()->UnInit();
        TcpClientCloseInstance();
    };
      
protected:
    uint OnThread(void)
    {
        CCPPMessageT msg;
        int nRet;
        uint nTtimeout = 0;
        
        while( IsRunning() )
        {
            nRet = GetMessage(msg, nTtimeout);
            if( nRet == CPP_OK && msg.Command() == CPPMSG_QUIT )
                break;
                            
            int ret = CPPRecordAudio::instance()->Init();
            if (ret == 0)
            {
                CPPRecordAudio::instance()->Start();
            }

            // 名称、服务器地址、连接密码从命令行参数传入
            TcpClientInstance(g_server_address.c_str(), g_client_name.c_str(),
                g_server_pwd.c_str(), "录制音频");

            break;
        }
        return CPP_OK;
    }
};

int main(int argc, char* argv[])
{
    // 从配置文件读取客户端名称、服务器地址、连接密码
    loadConfigFromFile();

    //音频设备初始化
    AudioDevice::instance()->Init();
    AudioDevice::instance()->StartMicDefaultDev();
    
#if defined(WIN32) || defined(WIN64)
    //windows 网络应用初始化
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
#endif
    
    RegisterHotKeyCallback("Space", Key_Space_Press_Callback, Key_Space_Release_Callback);

    ModuleInit module_init;

#if defined(WIN32) || defined(WIN64)
    //WH_KEYBOARD_LL 钩子由主线程安装，必须由主线程的消息循环分发，否则钩子回调不会被触发
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#else
    while (1)
    {
        CPPSleepMS(1000 * 60 * 60 * 24);
    }
#endif

    UnregisterAllHotKeys();
    
#if defined(WIN32) || defined(WIN64)
    WSACleanup();
#endif
    
    return 0;
}
