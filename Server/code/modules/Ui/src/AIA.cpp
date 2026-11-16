#if defined(WIN32) || defined(WIN64)
// winsock2.h 必须在 windows.h 之前包含，避免与 Server.h 中的 winsock2 重复定义冲突
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

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
#include <Windows.h>

#if defined (WIN32) | defined (WIN64)
#include <iphlpapi.h>
#else
#include <net/route.h>
#include <net/if.h>
#include <net/if_arp.h>
#endif

#include "GlobalShortCut.h"
#include "Server.h"

#include "nlohmann/json.hpp"
using json = nlohmann::json;

#include <QtCore/QTimer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QHeaderView>
#include <functional>

extern std::string g_username;
extern std::string g_passwd;
extern std::string g_server_pwd;
extern int g_dueDateYear;
extern int g_dueDateMonth;
extern int g_dueDateDay;

// 将g_server_pwd更新到server.cfg
static void UpdateServerPwdToCfg(const char* key, const char* val)
{
    char path[1024] = { 0 };
    char processname[1024] = { 0 };
    ICPPFile::GetExecutablePath(path, processname, sizeof(path));

    std::string cfgPath = std::string(path) + "server.cfg";

    // 先读取整个配置文件内容
    FILE* fp = nullptr;
    fopen_s(&fp, cfgPath.c_str(), "r");
    if (fp == nullptr)
    {
        printf("UpdateServerPwdToCfg fopen error: %s\n", cfgPath.c_str());
        return;
    }

    std::vector<std::string> vLines;
    bool bFound = false;

    if (fp)
    {
        char szBuf[1024];
        while (fgets(szBuf, sizeof(szBuf), fp))
        {
            std::string strLine = szBuf;
            // 移除末尾换行符
            while (!strLine.empty() && (strLine.back() == '\n' || strLine.back() == '\r'))
                strLine.pop_back();

            // 检查是否为server_passwd配置行
            if (strLine.find(key) == 0)
            {
                // 替换为新的密码
                char szNewLine[1024];
                snprintf(szNewLine, sizeof(szNewLine), "%s=%s", key, val);
                strLine = szNewLine;
                bFound = true;
            }
            vLines.push_back(strLine);
        }
        fclose(fp);
    }

    // 如果没有找到server_passwd行，追加到末尾
    if (!bFound)
    {
        char szNewLine[1024];
        snprintf(szNewLine, sizeof(szNewLine), "%s=%s", key, val);
        vLines.push_back(szNewLine);
    }

    // 写回配置文件
    fopen_s(&fp, cfgPath.c_str(), "w");
    if (fp == nullptr)
    {
        printf("UpdateServerPwdToCfg fopen error: %s\n", cfgPath.c_str());
        return;
    }
    
    if (fp)
    {
        for (size_t i = 0; i < vLines.size(); ++i)
        {
            fprintf(fp, "%s\n", vLines[i].c_str());
        }
        fclose(fp);
    }
}

#if defined (WIN32) | defined (WIN64)
int GetMac(CCPPString *pszMac)
{ 
	bool bRtn = false;
	PIP_ADAPTER_INFO pAdapterInfo;
	PIP_ADAPTER_INFO pAdapter = NULL;
	DWORD dwRetVal = 0;
	pAdapterInfo = (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));
	ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
	if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) != ERROR_SUCCESS)
	{
		free(pAdapterInfo);
		pAdapterInfo = (IP_ADAPTER_INFO *)malloc(ulOutBufLen);
	}
	if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR)
	{
		pAdapter = pAdapterInfo;
		char szMac[20] = { 0 };
		char szAddress[4] = { 0 };
		while (pAdapter)
		{
 
			if (strcmp(pAdapter->IpAddressList.IpAddress.String, "0.0.0.0") != 0)
			{
				for (UINT i = 0; i < pAdapter->AddressLength; i++)
				{
					sprintf_s(szAddress, "%02X", pAdapter->Address[i]);
					strcat_s(szMac, szAddress);
					if (i != pAdapter->AddressLength - 1)
					{
						strcat_s(szMac, ":");
					}
				}
				break;
			}
			pAdapter = pAdapter->Next;
		}
        
        pszMac->Append(szMac);
		bRtn = true;
	}
	return true;
}
#else
int GetMac(CCPPString *pszMac)
{
	struct ifaddrs	*ifaddr = NULL;
	struct ifaddrs	*ifp = NULL;
	int				fd;
	struct ifreq	ifr;
	const uchar		*mac;	
	int 			ret = getifaddrs(&ifaddr);

	if (ret) 
	{
		return -1;
	}

	ifp = ifaddr;
	for (; ifp != NULL; ifp = ifp->ifa_next) 
	{
		if ((ifp->ifa_addr && ifp->ifa_addr->sa_family == AF_INET) 
			&& (ifp->ifa_name && !CCPPString::IsEqual(ifp->ifa_name, "lo")))
		{
			fd = socket(AF_INET, SOCK_DGRAM, 0);
			if (fd < 0)
			{
				break;
			}

			memset(&ifr, 0, sizeof(ifr));
			strncpy(ifr.ifr_name, ifp->ifa_name, IFNAMSIZ - 1);
			ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
			ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
			close(fd);
			if (ret < 0)
			{
				break;
			}

			mac = (uchar*)ifr.ifr_hwaddr.sa_data;	
			
			pszMac->Append("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		}
	}

	freeifaddrs(ifaddr);	
	if (pszMac->IsEmpty())
	{
		return -1;
	}

	return 0;
}
#endif

void AIAShow()
{
    AIA* ins = AIA::instance();
    emit ins->SigShowWindow();
}

int AIAVIPCheck()
{
    AIA* ins = AIA::instance();
    if (ins->IsExpired())
    {
        return -1;
    }

    return 0;
}

AIA* AIA::instance()
{
	static AIA* _ins = NULL;
	if ( _ins == NULL )
	{
		_ins = new AIA();
	}

	return _ins;
}

AIA::AIA(QWidget* parent)
     : QMainWindow(parent)
     , m_screen_width(1920)
     , m_screen_height(1080)
     , m_scaleFactor(1.0)
     , m_centralWidget(NULL)
     , m_mainLayout(NULL)
     , m_titleLabel(NULL)
     , m_statusLabel(NULL)
     , m_login_button(NULL)
     , m_close_button(NULL)
	 , m_thread_worker(NULL)
	 , m_is_expired(1)
     , m_serviceQrLabel(NULL)
     , m_officialLinkLabel(NULL)
     , m_listenLabel(NULL)
     , m_macLabel(NULL)
     , m_clientTable(NULL)
     , m_masterTable(NULL)
     , m_clientTimer(NULL)
     , m_masterTimer(NULL)
     , m_wake_cb_registered(false)
     , m_modifyPwdButton(NULL)
     , m_serviceButton(NULL)
     , m_service_window(NULL)
     , m_connect_pwd_pushed(false)
{
    char path[1024] = {0};
    char processname[1024] = {0};
    ICPPFile::GetExecutablePath(path, processname, sizeof(path));
    QString rcc = path;
    rcc += "Qrc.rcc";
    QResource::registerResource(rcc);

    //去掉标题栏
    this->setWindowFlags(Qt::FramelessWindowHint);

    //窗口移到中间
    QDesktopWidget *desktop = QApplication::desktop();
    int currentScreenIndex = desktop->screenNumber(this);
    QList<QScreen *> screen_list = QGuiApplication::screens();
    if(currentScreenIndex < screen_list.count()) {
        QScreen *tem = screen_list[currentScreenIndex];
        QRect screen_rect  = tem->geometry();

        //获取到软件窗口所在屏幕的 宽 高 尺寸
        m_screen_width  = screen_rect.width();
        m_screen_height = screen_rect.height();
    }

    //根据屏幕尺寸（以 1920×1080 为基准）计算缩放因子，限制在 0.8 - 2.5 范围
    m_scaleFactor = qBound((qreal)0.8, (qreal)m_screen_height / 1080.0, (qreal)2.5);
    auto s = [&](int val) -> int { return qRound(val * m_scaleFactor); };

    //设置黑色背景
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, Qt::black);
    this->setPalette(palette);
    this->setAutoFillBackground(true);
    
    m_connect_pwd = g_server_pwd;

    // 主人根目录：<exe>/Config/Masters/
    m_mastersDir = CCPPString(path);
    m_mastersDir += "Config/Masters/";
    ICPPFile::MakeDir(m_mastersDir.GetBuffer());

    //创建中心 widget 和主布局
    m_centralWidget = new QWidget(this);
    this->setCentralWidget(m_centralWidget);
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setSpacing(s(15));
    m_mainLayout->setContentsMargins(s(40), s(25), s(40), s(25));

    // ========== 极客配色 ==========
    const char* geekGreen     = "#00ff41";
    const char* geekGreenDim   = "#00b32d";
    QString     geekFont       = "font-family: 'Consolas', 'Microsoft YaHei UI';";

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

    // ========== 顶部标题区 ==========
    m_titleLabel = new QLabel("AIA  SERVER", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: %2px; font-weight: bold; %3 }")
            .arg(geekGreen).arg(s(38)).arg(geekFont)
    );
    m_mainLayout->addWidget(m_titleLabel, 0, Qt::AlignCenter);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(labelStyle);
    m_mainLayout->addWidget(m_statusLabel, 0, Qt::AlignCenter);

    m_mainLayout->addSpacing(s(10));

    // ========== 三栏内容区 ==========
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(s(20));

    // ---------- 模块1：服务器信息 ----------
    QGroupBox* serverGroup = new QGroupBox("服务器信息", this);
    serverGroup->setStyleSheet(groupStyle);
    QVBoxLayout* serverLayout = new QVBoxLayout(serverGroup);
    serverLayout->setSpacing(s(10));
    serverLayout->setContentsMargins(s(20), s(14), s(20), s(20));

    m_listenLabel = new QLabel("● 监听：检测中...", serverGroup);
    m_listenLabel->setStyleSheet(labelStyle);
    serverLayout->addWidget(m_listenLabel);

    m_macLabel = new QLabel("MAC：--", serverGroup);
    m_macLabel->setStyleSheet(labelStyle);
    serverLayout->addWidget(m_macLabel);

    serverLayout->addStretch(1);

    QString btnStyle = QString(
        "QPushButton { background-color: #000000; color: %1; border: %2px solid %1; border-radius: %3px; font-size: %4px; font-weight: bold; %5 }"
        "QPushButton:hover { background-color: %1; color: #000000; }"
        "QPushButton:pressed { background-color: %6; color: #000000; }"
    ).arg(geekGreen).arg(s(2)).arg(s(6)).arg(s(15)).arg(geekFont).arg(geekGreenDim);

    m_modifyPwdButton = new QPushButton("修改连接密码", serverGroup);
    m_modifyPwdButton->setFixedHeight(s(40));
    m_modifyPwdButton->setFocusPolicy(Qt::NoFocus);
    m_modifyPwdButton->setStyleSheet(btnStyle);
    connect(m_modifyPwdButton, &QPushButton::clicked, this, &AIA::on_m_modify_password_clicked);
    serverLayout->addWidget(m_modifyPwdButton);

    m_serviceButton = new QPushButton("客服信息", serverGroup);
    m_serviceButton->setFixedHeight(s(40));
    m_serviceButton->setFocusPolicy(Qt::NoFocus);
    m_serviceButton->setStyleSheet(btnStyle);
    connect(m_serviceButton, &QPushButton::clicked, this, &AIA::on_m_service_clicked);
    serverLayout->addWidget(m_serviceButton);

    m_login_button = new QPushButton("登 录", serverGroup);
    m_login_button->setFixedHeight(s(40));
    m_login_button->setFocusPolicy(Qt::NoFocus);
    m_login_button->setStyleSheet(btnStyle);
    connect(m_login_button, &QPushButton::clicked, this, &AIA::on_m_login_clicked);
    serverLayout->addWidget(m_login_button);

    m_close_button = new QPushButton("隐藏窗口", serverGroup);
    m_close_button->setFixedHeight(s(40));
    m_close_button->setFocusPolicy(Qt::NoFocus);
    m_close_button->setStyleSheet(btnStyle);
    connect(m_close_button, &QPushButton::clicked, this, &AIA::on_m_close_clicked);
    m_close_button->installEventFilter(this);
    serverLayout->addWidget(m_close_button);

    contentLayout->addWidget(serverGroup);

    // ---------- 模块2：客户端信息 ----------
    QGroupBox* clientGroup = new QGroupBox("客户端信息", this);
    clientGroup->setStyleSheet(groupStyle);
    QVBoxLayout* clientLayout = new QVBoxLayout(clientGroup);
    clientLayout->setSpacing(s(8));
    clientLayout->setContentsMargins(s(15), s(14), s(15), s(15));

    QString tableStyle = QString(
        "QTableWidget { background-color: #000000; color: %1; gridline-color: %1; border: %2px solid %1; font-size: %3px; %4 }"
        "QTableWidget::item { padding: %5px; }"
        "QTableWidget::item:selected { background-color: %1; color: #000000; }"
        "QHeaderView { background-color: #000000; }"
        "QHeaderView::section { background-color: #000000; color: %1; border: %2px solid %1; padding: %5px %6px; font-weight: bold; %4 }"
        "QTableCornerButton::section { background-color: #000000; border: %2px solid %1; }"
    ).arg(geekGreen).arg(s(1)).arg(s(14)).arg(geekFont).arg(s(4)).arg(s(8));

    // 统一的表头样式（给 QHeaderView 单独再次指定，解决 QSS 后代选择器不穿透复合控件的问题）
    QString headerStyle = QString(
        "QHeaderView { background-color: #000000; }"
        "QHeaderView::section { background-color: #000000; color: %1; border: %2px solid %1; padding: %3px %4px; font-weight: bold; %5 }"
    ).arg(geekGreen).arg(s(1)).arg(s(4)).arg(s(8)).arg(geekFont);

    m_clientTable = new QTableWidget(0, 2, clientGroup);
    m_clientTable->setStyleSheet(tableStyle);
    m_clientTable->setHorizontalHeaderLabels(QStringList() << "名字" << "功能");
    m_clientTable->horizontalHeader()->setStyleSheet(headerStyle);
    m_clientTable->verticalHeader()->setStyleSheet(headerStyle);
    m_clientTable->verticalHeader()->setVisible(false);
    m_clientTable->horizontalHeader()->setStretchLastSection(true);
    m_clientTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_clientTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_clientTable->setFocusPolicy(Qt::NoFocus);
    m_clientTable->setColumnWidth(0, s(110));
    clientLayout->addWidget(m_clientTable);

    contentLayout->addWidget(clientGroup);

    // ---------- 模块3：主人信息 ----------
    QGroupBox* masterGroup = new QGroupBox("主人信息", this);
    masterGroup->setStyleSheet(groupStyle);
    QVBoxLayout* masterLayout = new QVBoxLayout(masterGroup);
    masterLayout->setSpacing(s(8));
    masterLayout->setContentsMargins(s(15), s(14), s(15), s(15));

    m_masterTable = new QTableWidget(0, 2, masterGroup);
    m_masterTable->setStyleSheet(tableStyle);
    m_masterTable->setHorizontalHeaderLabels(QStringList() << "主人" << "唤醒词");
    m_masterTable->horizontalHeader()->setStyleSheet(headerStyle);
    m_masterTable->verticalHeader()->setStyleSheet(headerStyle);
    m_masterTable->verticalHeader()->setVisible(false);
    m_masterTable->horizontalHeader()->setStretchLastSection(true);
    m_masterTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_masterTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_masterTable->setFocusPolicy(Qt::NoFocus);
    m_masterTable->setColumnWidth(0, s(120));
    masterLayout->addWidget(m_masterTable);

    contentLayout->addWidget(masterGroup);

    m_mainLayout->addLayout(contentLayout, 1);

    connect(this, SIGNAL(SigUpdateStatusLabel()), this, SLOT(UpdateStatusLabel()));
    connect(this, SIGNAL(SigShowWindow()), this, SLOT(ShowWindow()));
    connect(this, SIGNAL(SigHideWindow()), this, SLOT(HideWindow()));
    connect(this, SIGNAL(SigMastersChanged()), this, SLOT(RefreshMasters()));

    // ========== 刷新定时器 ==========
    m_clientTimer = new QTimer(this);
    m_clientTimer->setInterval(1000);
    connect(m_clientTimer, &QTimer::timeout, this, &AIA::RefreshClients);
    m_clientTimer->start();

    m_masterTimer = new QTimer(this);
    m_masterTimer->setInterval(2000);
    connect(m_masterTimer, &QTimer::timeout, this, &AIA::RefreshMasters);
    m_masterTimer->start();

    // 首次刷新 + 尝试注册唤醒词回调
    RefreshClients();
    RefreshMasters();

    m_thread_worker = new CCPPThreadWorker(2, "AIA");
    if(m_thread_worker != NULL)
    {
        void* cast = this;
        m_thread_worker->Attach((CCPPObject*)cast, (CPPTHREADAPI)&AIA::ThreadProc);
    }
}

AIA::~AIA()
{
    if(m_thread_worker != NULL)
    {
        void* cast = this;
        m_thread_worker->Detach((CCPPObject*)cast, (CPPTHREADAPI)&AIA::ThreadProc);
        CPPDel(m_thread_worker);
    }
}

void AIA::on_m_login_clicked()
{
    qreal  scale       = m_scaleFactor;
    auto   s           = [&](int v) -> int { return qRound(v * scale); };
    const char* green  = "#00ff41";
    const char* greenD = "#00b32d";
    QString geekFont   = "font-family: 'Consolas', 'Microsoft YaHei UI';";

    QDialog dialog(this);
    dialog.setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dialog.setAttribute(Qt::WA_TranslucentBackground, false);
    dialog.setFixedSize(s(460), s(360));

    QString dlgStyle = QString(
        "QDialog {"
        "   background-color: #000000;"
        "   border: %1px solid %2;"
        "   border-radius: %3px;"
        "}"
        "QLabel {"
        "   color: %2;"
        "   font-size: %4px;"
        "   %5"
        "}"
        "QLineEdit {"
        "   background-color: #000000;"
        "   color: %2;"
        "   border: %6px solid %2;"
        "   border-radius: %7px;"
        "   padding: %8px;"
        "   font-size: %4px;"
        "   selection-background-color: %2;"
        "   selection-color: #000000;"
        "   %5"
        "}"
        "QLineEdit:focus {"
        "   border: %6px solid %9;"
        "}"
        "QLineEdit:hover {"
        "   border: %6px solid %9;"
        "}"
        "QPushButton {"
        "   background-color: #000000;"
        "   color: %2;"
        "   border: %6px solid %2;"
        "   border-radius: %7px;"
        "   padding: %10px %11px;"
        "   font-size: %4px;"
        "   font-weight: bold;"
        "   min-height: %12px;"
        "   %5"
        "}"
        "QPushButton:hover {"
        "   background-color: %2;"
        "   color: #000000;"
        "}"
        "QPushButton:pressed {"
        "   background-color: %9;"
        "   color: #000000;"
        "}"
        "QDialogButtonBox QPushButton {"
        "   min-width: %13px;"
        "}"
    ).arg(s(2)).arg(green).arg(s(10))
     .arg(s(15)).arg(geekFont)
     .arg(s(2)).arg(s(6)).arg(s(8))
     .arg(greenD)
     .arg(s(10)).arg(s(24)).arg(s(38))
     .arg(s(120));

    dialog.setStyleSheet(dlgStyle);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(s(22));
    mainLayout->setContentsMargins(s(36), s(30), s(36), s(28));
    
    QLabel* titleLabel = new QLabel("AIA  登录");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        QString(
            "QLabel {"
            "   color: %1;"
            "   font-size: %2px;"
            "   letter-spacing: 2px;"
            "   font-weight: bold;"
            "   %3"
            "}"
        ).arg(green).arg(s(26)).arg(geekFont)
    );
    mainLayout->addWidget(titleLabel);

    QFrame* sep = new QFrame(&dialog);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("QFrame { color: %1; background-color: %1; max-height: %2px; min-height: %2px; border: none; }").arg(green).arg(s(1)));
    mainLayout->addWidget(sep);
    
    QFormLayout* formLayout = new QFormLayout();
    formLayout->setSpacing(s(16));
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setContentsMargins(s(6), s(4), s(6), s(4));
    
    QLineEdit* usernameEdit = new QLineEdit();
    usernameEdit->setPlaceholderText("请输入用户名");
    usernameEdit->setMinimumHeight(s(40));
    usernameEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    
    QLineEdit* passwordEdit = new QLineEdit();
    passwordEdit->setPlaceholderText("请输入密码");
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setMinimumHeight(s(40));
    passwordEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    
    formLayout->addRow("用户名:", usernameEdit);
    formLayout->addRow("密  码:", passwordEdit);
    
    mainLayout->addLayout(formLayout, 1);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setText("登  录");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("取  消");
    buttonBox->setCenterButtons(true);
    buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet(dlgStyle);
    buttonBox->button(QDialogButtonBox::Cancel)->setStyleSheet(dlgStyle);
    
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    mainLayout->addWidget(buttonBox);
    
    if (strlen(g_username.c_str()) > 0)
    {
        usernameEdit->setText(QString::fromUtf8(g_username.c_str()));
    }
    
    if (strlen(g_passwd.c_str()) > 0)
    {
        passwordEdit->setText(QString::fromUtf8(g_passwd.c_str()));
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        QString username = usernameEdit->text().trimmed();
        QString password = passwordEdit->text();
        
        if (username.isEmpty() || password.isEmpty())
        {
            QMessageBox::warning(this, "警告", "用户名和密码不能为空！");
            return;
        }
        
        if (username.length() >= 63 || password.length() >= 63)
        {
            QMessageBox::warning(this, "警告", "用户名和密码太长！");
            return;
        }
        
        // 初始化会员信息结构体
        MembershipInfo info = {
            "",      // status
            "",      // message
            true,    // is_expired
            "",      // membership_type
            "",      // expiry_time
            0,       // days_left
            true,    // mac_changed
            ""       // mac_change_message
        };
        GetMemInfo(username.toStdString().c_str(), password.toStdString().c_str(), &info);
    }
}

void AIA::on_m_close_clicked()
{
    emit SigHideWindow();
}

void AIA::on_m_modify_password_clicked()
{
    qreal scale = m_scaleFactor;
    auto s = [&](int v) -> int { return qRound(v * scale); };
    const char* green  = "#00ff41";
    const char* greenD = "#00b32d";
    QString geekFont   = "font-family: 'Consolas', 'Microsoft YaHei UI';";

    QDialog dlg(this);
    dlg.setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg.setAttribute(Qt::WA_TranslucentBackground, false);
    dlg.setFixedSize(s(440), s(240));
    dlg.setStyleSheet(
        QString(
            "QDialog { background-color:#000000; border:%1px solid %2; border-radius:%3px; }"
            "QLabel { color:%2; font-size:%4px; %5 }"
            "QLineEdit { background-color:#000; color:%2; border:%1px solid %2; border-radius:%6px; padding:%7px; font-size:%4px; selection-background-color:%2; selection-color:#000; %5 }"
            "QLineEdit:focus { border:%1px solid %8; }"
            "QLineEdit:hover { border:%1px solid %8; }"
            "QPushButton { background-color:#000; color:%2; border:%1px solid %2; border-radius:%6px; padding:%9px %10px; font-weight:bold; font-size:%4px; %5 }"
            "QPushButton:hover { background-color:%2; color:#000; }"
            "QPushButton:pressed { background-color:%8; color:#000; }"
        ).arg(s(2)).arg(green).arg(s(10)).arg(s(14)).arg(geekFont)
         .arg(s(6)).arg(s(6)).arg(greenD)
         .arg(s(8)).arg(s(24))
    );

    QVBoxLayout* lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(0, s(20), 0, s(20));
    lay->setSpacing(s(12));

    // 内容区
    QVBoxLayout* content = new QVBoxLayout();
    content->setContentsMargins(s(24), s(8), s(24), 0);
    content->setSpacing(s(12));
    QLabel* lbl = new QLabel("新的连接服务器密码：", &dlg);
    content->addWidget(lbl);

    QLineEdit* edit = new QLineEdit(&dlg);
    edit->setEchoMode(QLineEdit::Normal);          // 明文显示
    edit->setText(QString::fromStdString(m_connect_pwd));
    edit->setMinimumHeight(s(36));
    content->addWidget(edit);

    content->addStretch(1);

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(s(12));
    QPushButton* save = new QPushButton("保 存", &dlg);
    QPushButton* cancel = new QPushButton("取 消", &dlg);
    save->setMinimumWidth(s(110));
    cancel->setMinimumWidth(s(110));
    btnRow->addStretch(1);
    btnRow->addWidget(save);
    btnRow->addWidget(cancel);
    content->addLayout(btnRow);
    lay->addLayout(content, 1);

    QObject::connect(save, &QPushButton::clicked, [&]() {
        QString newPwd = edit->text();
        g_server_pwd = newPwd.toStdString();
        //写入配置文件
        UpdateServerPwdToCfg("server_passwd", g_server_pwd.c_str());

        m_connect_pwd = g_server_pwd;
        TCP_HTTP_Server* srv = get_server_instance();
        if (srv) srv->set_connect_password(m_connect_pwd);
        m_connect_pwd_pushed = true;

        if (m_statusLabel)
            m_statusLabel->setText(QString("<span style='color:#00ff41;'>连接密码已保存</span>"));
        dlg.accept();
    });
    QObject::connect(cancel, &QPushButton::clicked, [&]() { dlg.reject(); });

    dlg.exec();
}

void AIA::on_m_service_clicked()
{
    if (m_service_window == NULL)
    {
        qreal scale = m_scaleFactor;
        auto s = [&](int v) -> int { return qRound(v * scale); };
        const char* green  = "#00ff41";
        const char* greenD = "#00b32d";

        m_service_window = new QWidget(nullptr);
        m_service_window->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
        m_service_window->setAttribute(Qt::WA_TranslucentBackground, false);
        int winW = s(340);
        int winH = s(480);
        m_service_window->setFixedSize(winW, winH);
        m_service_window->setStyleSheet(
            QString(
                "QWidget#serviceWin { background-color:#000000; border:%1px solid %2; border-radius:%3px; }"
                "QLabel { color:%2; font-family:'Consolas','Microsoft YaHei UI'; }"
                "QPushButton { background-color:#000; color:%2; border:%1px solid %2; border-radius:%4px; padding:%5px; font-weight:bold; font-family:'Consolas','Microsoft YaHei UI'; }"
                "QPushButton:hover { background-color:%2; color:#000; }"
                "QPushButton:pressed { background-color:%6; color:#000; }"
            ).arg(s(2)).arg(green).arg(s(10)).arg(s(6)).arg(s(8)).arg(greenD)
        );
        m_service_window->setObjectName("serviceWin");

        QVBoxLayout* lay = new QVBoxLayout(m_service_window);
        lay->setContentsMargins(0, s(16), 0, s(16));
        lay->setSpacing(0);

        // 内容区
        QVBoxLayout* content = new QVBoxLayout();
        int m = s(16);
        content->setContentsMargins(m, s(14), m, m);
        content->setSpacing(s(10));

        QLabel* sub = new QLabel("客服微信", m_service_window);
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet(QString("color:%1; font-size:16px; font-weight:bold; font-family:'Consolas','Microsoft YaHei UI';").arg(green));
        content->addWidget(sub);

        QLabel* img = new QLabel(m_service_window);
        QPixmap pix(":/ico/wx.png");
        if (!pix.isNull())
        {
            int pw = s(260);
            QPixmap scaled = pix.scaledToWidth(pw, Qt::SmoothTransformation);
            img->setPixmap(scaled);
        }
        else
        {
            img->setText("未找到客服二维码");
            img->setStyleSheet("color:#ff5555; font-size:14px;");
        }
        img->setAlignment(Qt::AlignCenter);
        content->addWidget(img, 1);

        QPushButton* closeBtn = new QPushButton("关 闭", m_service_window);
        closeBtn->setMinimumHeight(s(38));
        QObject::connect(closeBtn, &QPushButton::clicked, [=]() { m_service_window->hide(); });
        content->addWidget(closeBtn);
        lay->addLayout(content, 1);

    }
    m_service_window->show();
    m_service_window->raise();
    m_service_window->activateWindow();
}

int AIA::ThreadProc(CCPPMessageT* msg, uint timestamp, uint& timeout)
{
    timeout = 1000;

    if(msg != NULL)
    {
        //线程启动调用一次
        if(CPPMSG_START == msg->Command())
        {
            if (strlen(g_username.c_str()) == 0 || strlen(g_passwd.c_str()) == 0)
            {
                SetStatusText("用户未登录，请访问官网注册账户后登录");
                emit SigUpdateStatusLabel();
                emit SigShowWindow();
                return 0;
            }
            else
            {
                CCPPString loginStr;
                loginStr.Append("用户 %s 登录中", g_username.c_str());
                SetStatusText(loginStr.GetBuffer());
                emit SigUpdateStatusLabel();
            }
            
            // 初始化会员信息结构体
            MembershipInfo info = {
                "",      // status
                "",      // message
                true,    // is_expired
                "",      // membership_type
                "",      // expiry_time
                0,       // days_left
                true,    // mac_changed
                ""       // mac_change_message
            };
            GetMemInfo(g_username.c_str(), g_passwd.c_str(), &info);
        }
    }
    else
    {
    }

    return 0;
}

void AIA::SetStatusText(const char* text)
{
    // 状态文本转成 HTML（把 \n 换成 <br>）
    CCPPString body = text;
    body.Replace("\n", "<br>");

    m_status_text.Empty();
    m_status_text.Append("<span style=\"color: #2ecc71;\"> %s </span>", body.GetBuffer());
}

void AIA::UpdateStatusLabel()
{
    if (m_statusLabel)
    {
        m_statusLabel->setText(m_status_text.GetBuffer());
    }
}

void AIA::ShowWindow()
{
    if (isHidden())
    {
        // 无边框窗口在 Windows 下 showMaximized() / setWindowState(Qt::WindowMaximized)
        // 都不可靠，改为手动把窗口几何设为屏幕工作区（伪最大化）
        QScreen* scr = QGuiApplication::primaryScreen();
        if (scr) setGeometry(scr->availableGeometry());
        show();
        activateWindow();
        raise();
    }
}

void AIA::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    // 无边框窗口：每次显示都手动铺满屏幕工作区
    QScreen* scr = QGuiApplication::primaryScreen();
    if (scr) setGeometry(scr->availableGeometry());
}

void AIA::HideWindow()
{
    if (!isHidden())
    {
        hide();
    }
}

bool AIA::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_close_button) {
        if (event->type() == QEvent::Enter) {
            m_close_button->setText("Ctrl+Alt+5显示");
            return true;
        }
        else if (event->type() == QEvent::Leave) {
            m_close_button->setText("隐藏窗口");
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void AIA::on_status_link_activated(const QString& link)
{
    // 用默认浏览器打开链接
    QDesktopServices::openUrl(QUrl(link));
    // 最小化当前窗口，避免遮挡浏览器
//  showMinimized();
}

void AIA::GetMemInfo(const char* username, const char* passwd, MembershipInfo* meminfo)
{
    if (strlen(username) == 0 || strlen(passwd) == 0)
    {
        return ;
    }
    
    int loginSuccess = 0;
    
    CCPPString host = "https://liangqin.cc";
    CCPPString path = "/api/check-membership";
    CCPPString mac;
    GetMac(&mac);
    
    CCPPString jsonData = "{\"username\":\"";
    jsonData += username;
    jsonData += "\",\"password\":\"";
    jsonData += passwd;
    if(mac.Length() > 0)
    {
        jsonData += "\",\"mac_address\":\"";
        jsonData += mac;
        jsonData += "\"}";
    }
    else
    {
        jsonData += "\",\"mac_address\":\"00-00-00-00-00-00\"}";
    }

    printf("pengfei json:%s\n", jsonData.GetBuffer());
    
    CCPPString response;
    bool success = HttpPost(host, path, jsonData, response, 10000);
    printf("pengfei success:%d\n", success);
    if (success)
    {
        std::string res = response.GetBuffer();
        printf("pengfei response: %s\n", res.c_str());
        
        try {
            auto j = json::parse(res);
            
            // 解析所有字段
            meminfo->status = j.value("status", std::string(""));
            meminfo->message = j.value("message", std::string(""));
            meminfo->is_expired = j.value("is_expired", true);
            meminfo->membership_type = j.value("membership_type", std::string(""));
            meminfo->expiry_time = j.value("expiry_time", std::string(""));
            meminfo->days_left = j.value("days_left", 0);
            meminfo->mac_changed = j.value("mac_changed", true);
            meminfo->mac_change_message = j.value("mac_change_message", std::string(""));
            
            printf("pengfei parsed:\n");
            printf("  status: %s\n", meminfo->status.c_str());
            CCPPString::AdaPrintf("  message: %s\n", meminfo->message.c_str());
            printf("  is_expired: %s\n", meminfo->is_expired ? "true" : "false");
            CCPPString::AdaPrintf("  membership_type: %s\n", meminfo->membership_type.c_str());
            printf("  expiry_time: %s\n", meminfo->expiry_time.c_str());
            printf("  days_left: %d\n", meminfo->days_left);
            printf("  mac_changed: %s\n", meminfo->mac_changed ? "true" : "false");
            CCPPString::AdaPrintf("  mac_change_message: %s\n", meminfo->mac_change_message.c_str());
            if(meminfo->status == "success")
            {
                loginSuccess = 1;
                
                g_username = username;
                g_passwd = passwd;

                int year, month, day, hour, minute, second;
                if (sscanf_s(meminfo->expiry_time.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6)
                {
                    g_dueDateYear = year;
                    g_dueDateMonth = month;
                    g_dueDateDay = day;
                }
            }

            if(loginSuccess)
            {
                //写文件
                UpdateServerPwdToCfg("username", g_username.c_str());
                UpdateServerPwdToCfg("password", g_passwd.c_str());
                UpdateServerPwdToCfg("due_date_year", std::to_string(g_dueDateYear).c_str());
                UpdateServerPwdToCfg("due_date_month", std::to_string(g_dueDateMonth).c_str());
                UpdateServerPwdToCfg("due_date_day", std::to_string(g_dueDateDay).c_str());
            }
        }
        catch (const std::exception& e) {
            printf("pengfei json parse error: %s\n", e.what());
        }
        
        CCPPString showMsg;
        if(loginSuccess)
        {
            if(!meminfo->is_expired && !meminfo->mac_changed)
            {
                m_is_expired = 0;
            }
            else
            {
                m_is_expired = 1;
            }
            if(meminfo->is_expired)
            {
                showMsg.Append("会员名：%s 到期时间：%s 会员状态：%s", username, meminfo->expiry_time.c_str(), meminfo->message.c_str());
            }
            else
            {
                if(meminfo->mac_changed)
                {
                    showMsg.Append("会员名：%s 到期时间：%s 会员类型：%s\n会员状态：%s %s", username, meminfo->expiry_time.c_str(), meminfo->membership_type.c_str(), meminfo->mac_change_message.c_str(), "请重启程序！");
                }
                else
                {
                    showMsg.Append("会员名：%s 到期时间：%s 会员类型：%s 会员状态：%s", username, meminfo->expiry_time.c_str(), meminfo->membership_type.c_str(), meminfo->message.c_str());
                }
            }
        }
        else
        {
            m_is_expired = 1;
            showMsg.Append("会员名：%s 会员状态：%s", username, meminfo->message.c_str());
        }
        SetStatusText(showMsg.GetBuffer());
        emit SigUpdateStatusLabel();
    }
    else
    {
        int year = 2026;
        int month = 11;
        int day = 11;

#if defined(WIN32) || defined(WIN64)
        // Windows 平台使用 localtime_s
        std::time_t now = std::time(nullptr);
        struct tm tm_now;
        localtime_s(&tm_now, &now);
        year = tm_now.tm_year + 1900;
        month = tm_now.tm_mon + 1;
        day = tm_now.tm_mday;
#else
        // Linux / macOS 平台使用 localtime_r
        std::time_t now = std::time(nullptr);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        year = tm_now.tm_year + 1900;
        month = tm_now.tm_mon + 1;
        day = tm_now.tm_mday;
#endif
        if(is_expired(g_dueDateYear, g_dueDateMonth, g_dueDateDay, year, month, day))
        {
            m_is_expired = 1;
        }
        else
        {
            m_is_expired = 0;
        }
        
        CCPPString showMsg;
        if(m_is_expired)
        {
            showMsg.Append("会员名：%s 会员状态：登录失败，已到期", username);
        }
        else
        {
            showMsg.Append("会员名：%s 会员状态：登录失败", username);
        }
        CCPPString::AdaPrintf("m_is_expired:%d msg:%s\n", m_is_expired, showMsg.GetBuffer());
        SetStatusText(showMsg.GetBuffer());
        emit SigUpdateStatusLabel();
    }

    return ;
}

bool AIA::is_expired(int due_year, int due_month, int due_day, int cur_year, int cur_month, int cur_day)
{
    if (due_year != cur_year)
        return due_year < cur_year;
    if (due_month != cur_month)
        return due_month < cur_month;
    return due_day < cur_day;
}

bool AIA::HttpPost(CCPPString& host, CCPPString& path, CCPPString& jsonData, CCPPString& response, int timeoutMs)
{
    CCPPHTTPSession* session = CCPPHTTPSession::Acquire();
    if (session == NULL)
        return false;
    
    CCPPString url = host;
    url += path;
    
    CCPPString content;
    int ret = session->Commit("POST", url.GetBuffer(), 
                              jsonData.GetBuffer(), strlen(jsonData.GetBuffer()),
                              "application/json", timeoutMs);
    
    if (ret < 0)
    {
        session->Release();
        return false;
    }
    
    CCPPHTTPCommand* resp = session->GetResponse();
    if (resp != NULL)
    {
        const char* body = resp->GetContent()->GetBuffer();
        if (body != NULL)
        {
            response = body;
        }
    }
    
    session->Release();
    return true;
}

void AIA::RegisterWakeWordCallbackIfReady()
{
    if (m_wake_cb_registered) return;
    TCP_HTTP_Server* srv = get_server_instance();
    if (srv == NULL) return;

    m_wake_cb_registered = true;
}

void AIA::NotifyMasterWakeWord(const std::string& master, const std::string& word)
{
    if (master.empty()) return;

    CCPPString masterDir = m_mastersDir;
    masterDir += master.c_str();
    masterDir += "/";
    ICPPFile::MakeDir(masterDir.GetBuffer());

    CCPPString wf = masterDir;
    wf += "wake_word.txt";
    QFile f(wf.GetBuffer());
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        QTextStream ts(&f);
        ts.setCodec("UTF-8");
        ts << QString::fromStdString(word);
        f.close();
    }

    // 跨线程通知 UI 刷新主人表（queued connection）
    emit SigMastersChanged();
}

void AIA::RefreshClients()
{
    RegisterWakeWordCallbackIfReady();

    TCP_HTTP_Server* srv = get_server_instance();
    const QColor green("#00ff41");

    if (srv == NULL || !srv->running.load())
    {
        if (m_listenLabel)
            m_listenLabel->setText("<span style='color:#ff5555;'>● 监听：未启动</span>");
        if (m_clientTable)
            m_clientTable->setRowCount(0);
        return;
    }

    if (srv && !m_connect_pwd_pushed)
    {
        srv->set_connect_password(m_connect_pwd);
        m_connect_pwd_pushed = true;
    }

    if (m_listenLabel)
        m_listenLabel->setText("<span style='color:#00ff41;'>● 监听：成功</span>"
                                " <span style='color:#00ff41;'>(" + QString::number(srv->tcp_port) + "/" + QString::number(srv->http_port) + ")</span>");

    // MAC 仅取一次
    if (m_macLabel && m_macLabel->text() == "MAC：--")
    {
        CCPPString mac;
        GetMac(&mac);
        m_macLabel->setText("MAC：" + QString::fromUtf8(mac.GetBuffer()));
    }

    if (m_clientTable == NULL) return;
    std::vector<ClientInfo> list = srv->get_clients_detail();
    m_clientTable->setRowCount((int)list.size());
    for (int i = 0; i < (int)list.size(); ++i)
    {
        const ClientInfo& ci = list[i];
        QTableWidgetItem* n = new QTableWidgetItem(QString::fromStdString(ci.name));
        QTableWidgetItem* c = new QTableWidgetItem(QString::fromStdString(ci.commands));
        n->setForeground(green); c->setForeground(green);
        m_clientTable->setItem(i, 0, n);
        m_clientTable->setItem(i, 1, c);
    }
}

void AIA::RefreshMasters()
{
    if (m_masterTable == NULL) return;

    QDir dir(m_mastersDir.GetBuffer());
    if (!dir.exists())
    {
        m_masterTable->setRowCount(0);
        return;
    }

    QStringList subs = dir.entryList(QStringList(), QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    const QColor green("#00ff41");

    m_masterTable->setRowCount(subs.size());
    for (int i = 0; i < subs.size(); ++i)
    {
        QString masterName = subs[i];
        QString wordPath = dir.absoluteFilePath(masterName) + "/wake_word.txt";
        QString word;
        QFile f(wordPath);
        if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream ts(&f);
            ts.setCodec("UTF-8");
            word = ts.readAll().trimmed();
            f.close();
        }
        QTableWidgetItem* n = new QTableWidgetItem(masterName);
        QTableWidgetItem* w = new QTableWidgetItem(word.isEmpty() ? QString("--") : word);
        n->setForeground(green); w->setForeground(green);
        m_masterTable->setItem(i, 0, n);
        m_masterTable->setItem(i, 1, w);
    }
}

