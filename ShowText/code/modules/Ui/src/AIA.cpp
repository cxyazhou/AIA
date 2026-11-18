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

void AIAShowTips(const char* text)
{
    ShowTipsGui::instance()->SetTips(text, 5);
}

ShowTipsGui* ShowTipsGui::instance()
{
	static ShowTipsGui* _ins = NULL;
	if ( _ins == NULL )
	{
		_ins = new ShowTipsGui();
	}

	return _ins;
}

ShowTipsGui::ShowTipsGui(QWidget* parent)
     : QWidget(parent)
     , m_screen_width(1920)
     , m_screen_height(1080)
     , m_scaleFactor(1.0)
	 , m_text_is_update(0)
	 , m_text_show_cnt(0)
     , m_text_show_label(NULL)
     , m_text_show_layout(NULL)
     , m_thread_worker(NULL)
{
    QDesktopWidget *desktop = QApplication::desktop();
    int currentScreenIndex = desktop->screenNumber(this);
    QList<QScreen *> screen_list = QGuiApplication::screens();
    if(currentScreenIndex < screen_list.count()) {
        QScreen *tem = screen_list[currentScreenIndex];
        QRect screen_rect  = tem->geometry();

        //获取到软件窗口所在屏幕的 宽 高 尺寸
        m_screen_width = screen_rect.width();
        m_screen_height = screen_rect.height();
    }

    //根据屏幕尺寸（以 1920×1080 为基准）计算缩放因子，限制在 0.8 - 2.5 范围
    m_scaleFactor = qBound((qreal)0.8, (qreal)m_screen_height / 1080.0, (qreal)2.5);
    
    // 创建透明窗口显示文字
    setWindowFlags(
        Qt::FramelessWindowHint |          // 无边框
        Qt::WindowStaysOnTopHint |         // 始终在最前面
        Qt::Tool |                         // 工具窗口，不在任务栏显示
        Qt::WindowTransparentForInput      // 窗口不接收鼠标事件
    );
    
    // 设置窗口属性为透明
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // 根据屏幕分辨率计算样式参数（使用 m_scaleFactor 进行适配）
    int     fontSize     = qRound(18  * m_scaleFactor);
    int     paddingV     = qRound(15  * m_scaleFactor);
    int     paddingH     = qRound(25  * m_scaleFactor);
    int     borderRadius = qRound(12  * m_scaleFactor);
    
    char    styleBuf[2048];
    sprintf_s(styleBuf, sizeof(styleBuf),
        "QLabel {"
        "   color: rgba(255, 255, 255, 0.95);"
        "   background-color: rgba(40, 40, 60, 0.85);"
        "   border: %dpx solid rgba(255, 255, 255, 0.15);"
        "   border-radius: %dpx;"
        "   padding: %dpx %dpx;"
        "   font-size: %dpx;"
        "   font-weight: 500;"
        "   font-family: Microsoft YaHei UI;"
        "   line-height: 1.5;"
        "}",
        qRound(1 * m_scaleFactor), borderRadius, paddingV, paddingH, fontSize
    );
    
    // 创建标签并设置样式
    m_text_show_label = new QLabel(this);
    m_text_show_label->setStyleSheet(styleBuf);
    // 设置标签文字对齐方式为居中
    m_text_show_label->setAlignment(Qt::AlignCenter);
    
    // 启用自动换行，防止长文本被截断
//  m_text_show_label->setWordWrap(true);
    
    // 设置文本格式为富文本，支持简单HTML
//  m_text_show_label->setTextFormat(Qt::RichText);
    
    // 创建布局
    m_text_show_layout = new QVBoxLayout(this);
    m_text_show_layout->setSpacing(0);
    m_text_show_layout->setContentsMargins(
        qRound(15 * m_scaleFactor), qRound(15 * m_scaleFactor),
        qRound(15 * m_scaleFactor), qRound(15 * m_scaleFactor)
    ); // 增加外边距，让阴影效果更明显
    m_text_show_layout->addWidget(m_text_show_label);
        
    connect(this, SIGNAL(SigHideShowGui()), this, SLOT(HideShowGui()));
        
    m_thread_worker = new CCPPThreadWorker(2, "ShowTipsGui");
    if(m_thread_worker != NULL)
    {
        void* cast = this;
        m_thread_worker->Attach((CCPPObject*)cast, (CPPTHREADAPI)&ShowTipsGui::ThreadProc);
    }
}

ShowTipsGui::~ShowTipsGui()
{
    if(m_thread_worker != NULL)
    {
        void* cast = this;
        m_thread_worker->Detach((CCPPObject*)cast, (CPPTHREADAPI)&ShowTipsGui::ThreadProc);
        CPPDel(m_thread_worker);
    }
}

void ShowTipsGui::SetTips(const char* text, int showTime)
{
    m_text = text;
    m_text_show_cnt = showTime * 5;
    m_text_is_update = 1;
}

int ShowTipsGui::ThreadProc(CCPPMessageT* msg, uint timestamp, uint& timeout)
{
    timeout = 200;

    if(msg != NULL)
    {
        //线程启动调用一次
        if(CPPMSG_START == msg->Command())
        {
            emit SigHideShowGui();
        }
    }
    else
    {
        emit SigHideShowGui();
    }

    return 0;
}

void ShowTipsGui::SetText(const char* text)
{
    m_text_show_label->setText(text);
    
    // 根据文字内容调整标签大小
    m_text_show_label->adjustSize();
        
    // 根据标签大小调整窗口大小
    resize(m_text_show_label->size());
    
    // 获取屏幕信息并居中显示在屏幕底部
    QDesktopWidget *desktop = QApplication::desktop();
    int currentScreenIndex = desktop->screenNumber(this);
    QList<QScreen *> screen_list = QGuiApplication::screens();
    if(currentScreenIndex < screen_list.count())
    {
        QScreen *screen = screen_list[currentScreenIndex];
        QRect screenRect = screen->availableGeometry();  // 使用可用区域（减去任务栏）
        
        int screenWidth = screenRect.width();
        int screenHeight = screenRect.height();
        int screenX = screenRect.x();
        int screenY = screenRect.y();
        
        int windowWidth = width();
        int windowHeight = height();
        
        // 根据 m_scaleFactor 计算底部边距
        int bottomMargin = qRound(20 * m_scaleFactor);
        
        // 计算位置：水平居中，垂直方向在底部（留一些边距）
        int posX = screenX + (screenWidth - windowWidth) / 2;
        int posY = screenY + screenHeight - windowHeight - bottomMargin;
        
        int maxWidth = screenWidth * 0.8; // 最大宽度为屏幕宽度的80%
        m_text_show_label->setMaximumWidth(maxWidth);

        move(posX, posY);
    }
}

void ShowTipsGui::HideShowGui()
{
    if(m_text_is_update)
    {
        m_text_is_update = 0;
        SetText(m_text.GetBuffer());
        show();
    }
    
    if(m_text_show_cnt > 0)
    {
        m_text_show_cnt--;
    }
    if(!m_text_show_cnt)
    {
        if(!isHidden())
        {
            hide();
        }
    }
}

