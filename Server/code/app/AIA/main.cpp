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

#pragma comment(lib, "Ui.lib")
#pragma comment(lib, "Server.lib")
#pragma comment(lib, "GlobalShortCut.lib")

#pragma comment(lib, "CPPBase.lib")

#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "libssl.lib")

#pragma comment(lib, "Qt5Core.lib")
#pragma comment(lib, "Qt5Gui.lib")
#pragma comment(lib, "Qt5Widgets.lib")
#endif //defined(WIN32) || defined(WIN64)

#if defined(WIN32) || defined(WIN64)
// winsock2.h 必须在 windows.h 之前包含，避免与 Server.h 中的 winsock2 重复定义冲突
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <QtWidgets/QApplication>

#include "GlobalShortCut.h"
#include "AIA.h"
#include "Server.h"
#include "CPPBase.h"

#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <cctype>

// 客户端配置（名称 / 服务器地址 / 连接密码），从 exe 同目录 server.cfg 读取
static std::string g_client_name = "音箱";
static std::string g_server_address = "127.0.0.1";

std::string g_username = "AIAUsername";
std::string g_passwd = "AIAPasswd";
std::string g_server_pwd = "MyAIAServer";
int g_dueDateYear = 2026;
int g_dueDateMonth = 11;
int g_dueDateDay = 11;

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

void Key_Ctrl_Alt_5_Press_Callback()
{
    AIAShow();
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
        server_stop();
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
            
            server_start();
                        
            break;
        }
        return CPP_OK;
    }
};

int main(int argc, char* argv[])
{
    char path[1024] = { 0 };
    char processname[1024] = { 0 };
    ICPPFile::GetExecutablePath(path, processname, sizeof(path));

    QApplication app(argc, argv);
        
#if defined(WIN32) || defined(WIN64)
    //windows 网络应用初始化
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
#endif
    
    RegisterHotKeyCallback("Ctrl+Alt+5", Key_Ctrl_Alt_5_Press_Callback, []() {});
        
    //主界面启动
    AIA* instance = AIA::instance();
    if (NULL != instance)
    {
        instance->show();
    }
    
    ModuleInit module_init;
    
    app.exec();
    
    UnregisterAllHotKeys();
    
#if defined(WIN32) || defined(WIN64)
    WSACleanup();
#endif
    
    return 0;
}
