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

class ShowTipsGui : public QWidget, public CCPPObject
{
	Q_OBJECT

public:
    static ShowTipsGui* instance();
    
	ShowTipsGui(QWidget* parent = Q_NULLPTR);
	~ShowTipsGui();

    void SetTips(const char* text, int showTime);
    
private:
    int ThreadProc(CCPPMessageT* msg, uint timestamp, uint& timeout);
    void SetText(const char* text);
    
signals:
    //信号最好不要加入参，因为emit后入参可能已经销毁了
    void SigHideShowGui();

private slots:
    void HideShowGui();

public:

private:
    int                 m_screen_width;
    int                 m_screen_height;
    qreal               m_scaleFactor;
    
    int                 m_text_is_update;
    CCPPString          m_text;
    int                 m_text_show_cnt;
    QLabel*             m_text_show_label;
    QVBoxLayout*        m_text_show_layout;
    
    CCPPThreadWorker*   m_thread_worker;
};

