#if defined(WIN32) || defined(WIN64)
#pragma comment(linker,"/subsystem:windows /entry:mainCRTStartup")

#pragma comment(lib, "libvlc.lib")
#pragma comment(lib, "libvlccore.lib")

#pragma comment(lib, "Ui.lib")

#pragma comment(lib, "Qt5Core.lib")
#pragma comment(lib, "Qt5Gui.lib")
#pragma comment(lib, "Qt5Widgets.lib")
#endif //defined(WIN32) || defined(WIN64)

#include <QtWidgets/QApplication>
#include <QtGui/QFont>

#include "mediaapp.h"
#include "common.h"

int main(int argc, char* argv[])
{
    // Linux 强制走 X11 (xcb)。libvlc 的 set_xwindow 只认 X11 原生窗口 ID，
    // Wayland 下 winId() 返回的不是 X11 Window，VLC 会忽略 set_xwindow 自己开窗口。
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    qputenv("QT_QPA_PLATFORM", "xcb");
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("媒体中心");
    app.setFont(QFont(FONT_FAMILY, 10));

    MediaApp win;
    win.show();

    return app.exec();
}
