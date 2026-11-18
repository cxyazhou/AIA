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
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QMenu>

#include "CPPBase.h"

class AIA : public QMainWindow, public CCPPObject
{
    Q_OBJECT

public:
    static AIA* instance();

    AIA(QWidget* parent = Q_NULLPTR);
    ~AIA();

    void AutoCmdRun();
    bool IsAutoStartRegistered();

protected:
    void closeEvent(QCloseEvent* event) override;
    void hideEvent(QHideEvent* event) override;

signals:
    //一条自动命令运行结束（后台线程发出，主线程刷新界面）
    void sigCmdRan(const QString& cmd);

private slots:
    void on_auto_start_changed(int state);
    void onCmdRan(const QString& cmd);
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayQuit();

public:
    qreal GetScaleFactor() const { return m_scaleFactor; }
    int   GetScreenWidth()  const { return m_screen_width; }
    int   GetScreenHeight() const { return m_screen_height; }

private:
    int   m_screen_width;
    int   m_screen_height;
    qreal m_scaleFactor;

    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QCheckBox* m_autoStartCheckBox;
    QCheckBox* m_killOnExitCheckBox;
    QTextEdit* m_runLog;
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_trayMenu;
    bool m_realQuit;
    QList<quintptr> m_cmdProcesses;
};
