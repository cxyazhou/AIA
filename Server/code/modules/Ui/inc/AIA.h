#pragma once

#include <QtCore/qthread.h>
#include <QtCore/qfile.h>
#include <QtCore/qdebug.h>
#include <QtCore/QVariant>

#include <QtGui/QMouseEvent>
#include <QtGui/QImage>

#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/QPushButton>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QtWidgets>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/QTextEdit>

#include "CPPBase.h"

// 会员信息结构体
typedef struct
{
    std::string status;
    std::string message;
    bool        is_expired;
    std::string membership_type;
    std::string expiry_time;
    int         days_left;
    bool        mac_changed;
    std::string mac_change_message;
} MembershipInfo;

void AIAShow();

class AIA : public QMainWindow, public CCPPObject
{
	Q_OBJECT

public:
    static AIA* instance();
    
	AIA(QWidget* parent = Q_NULLPTR);
	~AIA();

    qreal GetScaleFactor() const { return m_scaleFactor; }
    int   GetScreenWidth()  const { return m_screen_width; }
    int   GetScreenHeight() const { return m_screen_height; }
    
    int   IsExpired(void) { return m_is_expired; };
    
private:
    int ThreadProc(CCPPMessageT* msg, uint timestamp, uint& timeout);

    void GetMemInfo(const char* username, const char* passwd, MembershipInfo* meminfo);
    bool is_expired(int due_year, int due_month, int due_day, int cur_year, int cur_month, int cur_day);
    bool HttpPost(CCPPString& host, CCPPString& path, CCPPString& jsonData, CCPPString& response, int timeoutMs);

    void NotifyMasterWakeWord(const std::string& master, const std::string& word);
    void RegisterWakeWordCallbackIfReady();

    bool eventFilter(QObject* obj, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    
signals:
    //信号最好不要加入参，因为emit后入参可能已经消耗了
    void SigUpdateStatusLabel();
    void SigShowWindow();
    void SigHideWindow();
    void SigClientsChanged();
    void SigMastersChanged();

private slots:
	void on_m_login_clicked();
	void on_m_close_clicked();
    void on_status_link_activated(const QString& link);

    void SetStatusText(const char* text);
    void UpdateStatusLabel();
    void ShowWindow();
    void HideWindow();

    void RefreshClients();
    void RefreshMasters();

    void on_m_modify_password_clicked();
    void on_m_service_clicked();

public:

private:
    int   m_screen_width;
    int   m_screen_height;
    qreal m_scaleFactor;

    int   m_hide_cnt;
    
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QLabel* m_titleLabel;
    CCPPString m_status_text;
    QLabel* m_statusLabel;
    QPushButton *m_login_button;
    QPushButton *m_close_button;

    CCPPThreadWorker*   m_thread_worker;
    int                 m_is_expired;
    QLabel*             m_serviceQrLabel;
    QLabel*             m_officialLinkLabel;
    QLineEdit*          m_serverAddrInput;
    QPushButton*        m_serverAddrEditBtn;

    // ---- 3 模块新增成员 ----
    QLabel*             m_listenLabel;       // 监听状态
    QLabel*             m_macLabel;           // MAC 地址
    QTableWidget*       m_clientTable;       // 客户端信息表（名字/功能）
    QTableWidget*       m_masterTable;       // 主人信息表（主人/唤醒词）
    QTimer*             m_clientTimer;       // 客户端表刷新定时器
    QTimer*             m_masterTimer;       // 主人表刷新定时器
    CCPPString          m_mastersDir;         // 主人根目录 <exe>/Config/Masters/
    bool                m_wake_cb_registered; // 唤醒词回调是否已注册

    QPushButton*        m_modifyPwdButton;    // 修改连接服务器密码
    QPushButton*        m_serviceButton;      // 客服信息（wx.png 二维码）
    QWidget*            m_service_window;     // 客服二维码弹窗
    std::string         m_connect_pwd;        // 当前连接服务器密码（缓存，推送给 server）
    bool                m_connect_pwd_pushed; // 连接密码是否已推送给 server
};

