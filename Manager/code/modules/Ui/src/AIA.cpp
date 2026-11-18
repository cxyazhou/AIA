#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>

#include "AIA.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QMessageBox>
#include <QtGui/QDesktopServices>
#include <QtGui/QPixmap>
#include <QtCore/QUrl>

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#include <tchar.h>

void* StartProcess(const char* lpszCommand)
{
    // 构建命令行
    std::string cmdLine = lpszCommand;
    std::cout << cmdLine << std::endl;

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    // 使用 CreateProcessA (ANSI 版本)
    BOOL bSuccess = CreateProcessA(
        NULL,                    // 应用程序名
        (LPSTR)cmdLine.c_str(),  // 命令行
        NULL,                    // 进程安全属性
        NULL,                    // 线程安全属性
        FALSE,                   // 句柄继承
        0,                       // 创建标志
        NULL,                    // 环境块
        NULL,                    // 当前目录
        &si,                     // STARTUPINFO
        &pi                      // PROCESS_INFORMATION
    );

    if (bSuccess) {
        CloseHandle(pi.hThread);
        return pi.hProcess;      // 返回进程句柄，调用方负责关闭
    }
    printf("bSuccess:%d\n", bSuccess);
    return NULL;
}
#else
void* StartProcess(const char* lpszCommand)
{
    system(lpszCommand);
    return NULL;
}
#endif

AIA* AIA::instance()
{
    static AIA* _ins = NULL;
    if (_ins == NULL)
    {
        _ins = new AIA();
    }

    return _ins;
}

AIA::AIA(QWidget* parent)
    : QMainWindow(parent)
    , m_centralWidget(NULL)
    , m_mainLayout(NULL)
    , m_autoStartCheckBox(NULL)
    , m_killOnExitCheckBox(NULL)
    , m_runLog(NULL)
    , m_trayIcon(NULL)
    , m_trayMenu(NULL)
    , m_realQuit(false)
{
    char path[1024] = {0};
    char processname[1024] = {0};
    ICPPFile::GetExecutablePath(path, processname, sizeof(path));
    QString rcc = path;
    rcc += "Qrc.rcc";
    bool regOk = QResource::registerResource(rcc);
    qDebug() << "registerResource result:" << regOk;
    if (!regOk) {
        qWarning() << "Failed to register resource:" << rcc;
    }

    //窗口移到中间
    QDesktopWidget* desktop = QApplication::desktop();
    int currentScreenIndex = desktop->screenNumber(this);
    QList<QScreen*> screen_list = QGuiApplication::screens();
    if (currentScreenIndex < screen_list.count()) {
        QScreen* tem = screen_list[currentScreenIndex];
        QRect screen_rect = tem->geometry();

        //获取到软件窗口所在屏幕的 宽 高 尺寸
        m_screen_width = screen_rect.width();
        m_screen_height = screen_rect.height();

        resize(m_screen_width * 0.618, m_screen_width * 0.618 * 0.618);

        //移动窗口到 居中
        this->move((screen_rect.width() - this->width()) / 2, (screen_rect.height() - this->height()) / 2);
    }

    //根据屏幕尺寸（以 1920×1080 为基准）计算缩放因子，限制在 0.8 - 2.5 范围
    m_scaleFactor = qBound((qreal)0.8, (qreal)m_screen_height / 1080.0, (qreal)2.5);
    auto s = [&](int val) -> int { return qRound(val * m_scaleFactor); };

    //设置黑色背景
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, Qt::black);
    this->setPalette(palette);
    this->setAutoFillBackground(true);

    //创建中心 widget 和主布局
    m_centralWidget = new QWidget(this);
    this->setCentralWidget(m_centralWidget);
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setSpacing(s(15));
    m_mainLayout->setContentsMargins(s(40), s(25), s(40), s(25));

    // ========== 极客配色 ==========
    const char* geekGreen = "#00ff41";
    const char* geekGreenDim = "#00b32d";
    QString     geekFont = "font-family: 'Consolas', 'Microsoft YaHei UI';";

    QString groupStyle = QString(
        "QGroupBox {"
        "   background-color: #000000;"
        "   border: %1px solid %2;"
        "   border-radius: %3px;"
        "   margin-top: %4px;"
        "   padding-top: %5px;"
        "}"
        "QGroupBox::title {"
        "   subcontrol-origin: margin;"
        "   subcontrol-position: top center;"
        "   padding: 0 %6px;"
        "   color: %2;"
        "   font-size: %7px;"
        "   font-weight: bold;"
        "   %8"
        "}"
    ).arg(s(2)).arg(geekGreen).arg(s(6)).arg(s(26)).arg(s(6)).arg(s(12)).arg(s(16)).arg(geekFont);

    QString labelStyle = QString("QLabel { color: %1; font-size: %2px; %3 }")
        .arg(geekGreen).arg(s(15)).arg(geekFont);

    QString checkBoxStyle = QString(
        "QCheckBox { color: %1; font-size: %2px; spacing: %3px; %4 }"
        "QCheckBox::indicator { width: %5px; height: %5px; border-radius: %6px; border: %7px solid %1; background-color: #000000; }"
        "QCheckBox::indicator:checked { background-color: %1; border: %7px solid %1; }"
    ).arg(geekGreen).arg(s(14)).arg(s(10)).arg(geekFont).arg(s(20)).arg(s(3)).arg(s(2));

    QGroupBox* funcGroup = new QGroupBox("功能设置", this);
    funcGroup->setStyleSheet(groupStyle);
    QVBoxLayout* funcLayout = new QVBoxLayout(funcGroup);
    funcLayout->setSpacing(s(10));
    funcLayout->setContentsMargins(s(20), s(14), s(20), s(20));

    m_autoStartCheckBox = new QCheckBox("开机自动启动", funcGroup);
    m_autoStartCheckBox->setFocusPolicy(Qt::NoFocus);
    m_autoStartCheckBox->setStyleSheet(checkBoxStyle);

    m_autoStartCheckBox->setChecked(IsAutoStartRegistered());

    connect(m_autoStartCheckBox, &QCheckBox::stateChanged,
        this, &AIA::on_auto_start_changed);

    QHBoxLayout* row1 = new QHBoxLayout();
    row1->setSpacing(s(10));
    row1->addWidget(m_autoStartCheckBox);
    row1->addStretch(1);

    funcLayout->addLayout(row1);

    //退出时终止所有命令复选框，默认选中
    m_killOnExitCheckBox = new QCheckBox("退出系统后终止所有命令", funcGroup);
    m_killOnExitCheckBox->setFocusPolicy(Qt::NoFocus);
    m_killOnExitCheckBox->setStyleSheet(checkBoxStyle);
    m_killOnExitCheckBox->setChecked(true);

    QHBoxLayout* row2 = new QHBoxLayout();
    row2->setSpacing(s(10));
    row2->addWidget(m_killOnExitCheckBox);
    row2->addStretch(1);

    funcLayout->addLayout(row2);

    //功能设置分组加入主布局（此前遗漏，导致界面空白）
    m_mainLayout->addWidget(funcGroup);

    // ========== 运行记录分组 ==========
    QGroupBox* logGroup = new QGroupBox("运行记录", this);
    logGroup->setStyleSheet(groupStyle);
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    logLayout->setSpacing(s(8));
    logLayout->setContentsMargins(s(20), s(14), s(20), s(20));

    m_runLog = new QTextEdit(logGroup);
    m_runLog->setReadOnly(true);
    m_runLog->setFocusPolicy(Qt::NoFocus);
    m_runLog->setStyleSheet(QString(
        "QTextEdit {"
        "   background-color: #000000;"
        "   border: %1px solid %2;"
        "   border-radius: %3px;"
        "   color: %2;"
        "   font-size: %4px;"
        "   padding: %5px;"
        "   %6"
        "}"
    ).arg(s(1)).arg(geekGreenDim).arg(s(4)).arg(s(14)).arg(s(8)).arg(geekFont));

    logLayout->addWidget(m_runLog);

    //运行记录区域随窗口拉伸，填充剩余空间
    m_mainLayout->addWidget(logGroup, 1);

    //后台线程每跑完一条命令，主线程追加显示"已运行：xxx"
    connect(this, &AIA::sigCmdRan, this, &AIA::onCmdRan, Qt::QueuedConnection);

    // ========== 系统托盘 ==========
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/ico/AIA.png"));
    m_trayIcon->setToolTip(QStringLiteral("AIA Manager"));

    //托盘右键菜单：退出
    m_trayMenu = new QMenu(this);
    m_trayMenu->setStyleSheet(QString(
        "QMenu { background-color: #000000; border: %1px solid %2; color: %2; %3 }"
        "QMenu::item:selected { background-color: %2; color: #000000; }"
    ).arg(s(1)).arg("#00ff41").arg(geekFont));
    QAction* quitAction = m_trayMenu->addAction(QStringLiteral("退出"));
    connect(quitAction, &QAction::triggered, this, &AIA::onTrayQuit);
    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated,
        this, &AIA::onTrayActivated);
    m_trayIcon->show();
}

AIA::~AIA()
{
}

void AIA::closeEvent(QCloseEvent* event)
{
    if (!m_realQuit)
    {
        //点关闭按钮只是隐藏到托盘
        event->ignore();
        this->hide();
        return;
    }

    //真正退出时，仅在勾选"关闭时终止所有命令"时才终止进程
#if defined(WIN32) || defined(WIN64)
    if (m_killOnExitCheckBox && m_killOnExitCheckBox->isChecked())
    {
        for (quintptr h : m_cmdProcesses)
        {
            HANDLE hProc = (HANDLE)h;
            if (hProc)
            {
                TerminateProcess(hProc, 0);
                CloseHandle(hProc);
            }
        }
        m_cmdProcesses.clear();
    }
#endif
    QMainWindow::closeEvent(event);
}

void AIA::onTrayQuit()
{
    //托盘菜单退出：标记真正退出，然后关闭窗口
    m_realQuit = true;
    this->close();
    qApp->quit();
}

void AIA::hideEvent(QHideEvent* event)
{
    //窗口隐藏（最小化）时收到托盘
    if (m_trayIcon && isMinimized())
        m_trayIcon->show();
    QMainWindow::hideEvent(event);
}

void AIA::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    //双击托盘图标恢复窗口
    if (reason == QSystemTrayIcon::DoubleClick)
    {
        this->showNormal();
        this->activateWindow();
        this->raise();
    }
}

void AIA::on_auto_start_changed(int state)
{
    //设置开机自启动
    if (state == Qt::Checked)
    {
#if defined(WIN32) || defined(WIN64)
        //获取程序路径
        char path[MAX_PATH] = { 0 };
        GetModuleFileNameA(NULL, path, MAX_PATH);

        //注册表设置自启动
        HKEY hKey;
        LONG ret = RegOpenKeyExA(HKEY_CURRENT_USER,
            "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &hKey);
        if (ret == ERROR_SUCCESS)
        {
            RegSetValueExA(hKey, "AIA_MANAGER", 0, REG_SZ, (BYTE*)path, strlen(path) + 1);
            RegCloseKey(hKey);
        }
#endif
    }
    else
    {
#if defined(WIN32) || defined(WIN64)
        //取消自启动
        HKEY hKey;
        LONG ret = RegOpenKeyExA(HKEY_CURRENT_USER,
            "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &hKey);
        if (ret == ERROR_SUCCESS)
        {
            RegDeleteValueA(hKey, "AIA_MANAGER");
            RegCloseKey(hKey);
        }
#endif
    }
}

void AIA::onCmdRan(const QString& cmd)
{
    //一条命令运行结束，追加到运行记录
    if (m_runLog)
    {
        m_runLog->append(QStringLiteral("已运行：") + cmd);
    }
}

void AIA::AutoCmdRun()
{
    //读取exe目录下的 auto_cmd.cfg
    char path[1024] = { 0 };
    char processname[1024] = { 0 };
    ICPPFile::GetExecutablePath(path, processname, sizeof(path));

    CCPPString cmdFile(path);
    cmdFile += "auto_cmd.cfg";

#if defined(WIN32) || defined(WIN64)
    CCPPString tmpGbk;
    tmpGbk.Init(1024);
    CCPPString::utf8ToGbk(cmdFile.GetBuffer(), strlen(cmdFile.GetBuffer()), tmpGbk.GetBuffer(), 1024);
    std::ifstream file(tmpGbk.GetBuffer());
#else
    std::ifstream file(cmdFile.GetBuffer());
#endif

    std::string line;
    while (std::getline(file, line))
    {
        // 跳过空行
        if (line.empty())
            continue;
        // 去除行尾的 \r（Windows 换行兼容）
        if (line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        //界面显示用：文件按 UTF-8 解析
        QString cmdText = QString::fromUtf8(line.c_str());

#if defined(WIN32) || defined(WIN64)
        tmpGbk.Empty();
        CCPPString::utf8ToGbk(line.c_str(), strlen(line.c_str()), tmpGbk.GetBuffer(), 1024);
        printf("auto run:%s\n", tmpGbk.GetBuffer());

        void* hProc = StartProcess(tmpGbk.GetBuffer());
        if (hProc)
            m_cmdProcesses.append((quintptr)hProc);
#else
        StartProcess(line.c_str());
#endif
        //本条命令已运行结束，通知界面显示
        emit sigCmdRan(cmdText);

        if (cmdText.contains("AIA_Server"))
        {
            CPPSleepMS(5000);
        }
    }
    file.close();
}

bool AIA::IsAutoStartRegistered()
{
#if defined(WIN32) || defined(WIN64)
    char path[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, path, MAX_PATH);

    HKEY hKey = NULL;
    LONG ret = RegOpenKeyExA(HKEY_CURRENT_USER,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_QUERY_VALUE, &hKey);
    if (ret != ERROR_SUCCESS)
        return false;

    char oldPath[MAX_PATH] = { 0 };
    DWORD type = 0;
    DWORD size = sizeof(oldPath);

    LONG qret = RegQueryValueExA(hKey, "AIA_MANAGER", NULL, &type,
        (BYTE*)oldPath, &size);
    RegCloseKey(hKey);

    if (qret != ERROR_SUCCESS || type != REG_SZ)
        return false;

    return _stricmp(oldPath, path) == 0;
#else
    return false;
#endif
}