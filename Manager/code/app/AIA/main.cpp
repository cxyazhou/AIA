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
#pragma comment(lib, "CPPBase.lib")

#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "libssl.lib")

#pragma comment(lib, "Qt5Core.lib")
#pragma comment(lib, "Qt5Gui.lib")
#pragma comment(lib, "Qt5Widgets.lib")

#endif //defined(WIN32) || defined(WIN64)

#include <QtWidgets/QApplication>

#include "AIA.h"
#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <cctype>

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

            AIA* instance = AIA::instance();
            if (NULL != instance)
            {
                instance->AutoCmdRun();
            }

            break;
        }
        return CPP_OK;
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
        
    //主界面启动
    AIA* instance = AIA::instance();
    if (NULL != instance)
    {
        instance->hide();
    }
    
    ModuleInit module_init;
    
    app.exec();
        
    return 0;
}
