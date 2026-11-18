#include "viewers.h"
#include "mediaapp.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QShowEvent>
#include <QtGui/QScreen>
#include <QtGui/QPixmap>
#include <QtCore/QTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDebug>

// ---------- MSVC 补 ssize_t（放在 vlc 之前） ----------
#if defined(_MSC_VER)
#  include <cstddef>
   typedef ptrdiff_t ssize_t;
#else
#  include <sys/types.h>
#endif

#include <vlc/vlc.h>

// ---------- VLC 内部日志回调：把 VLC 的诊断打到 Qt 日志 ----------
#include <cstdarg>
#include <cstdio>
static void vlcLogCb(void* /*data*/, int level,
                     const libvlc_log_t* /*ctx*/,
                     const char* fmt, va_list args)
{
    const char* lvl = "info ";
    if      (level & LIBVLC_ERROR)   lvl = "ERROR";
    else if (level & LIBVLC_WARNING) lvl = "WARN ";
    else if (level & LIBVLC_DEBUG)   lvl = "debug";
    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, args);
    buf[sizeof(buf) - 1] = '\0';
    qDebug().noquote() << QString("[vlc %1] %2").arg(lvl, buf);
}

// ---------- VLC 事件回调 ----------
static void onVlcEnded(const libvlc_event_t *, void *data)
{
    auto *self = static_cast<PlayerWindow*>(data);
    QTimer::singleShot(0, self, [self](){ self->handleEnded(); });
}
static void onVlcError(const libvlc_event_t *, void *data)
{
    auto *self = static_cast<PlayerWindow*>(data);
    QTimer::singleShot(0, self, [self](){ self->handleError(); });
}

// 把本地路径转成 VLC 能吃的 file:// URI（UTF-8 字节，正斜杠）
// 标准 file URI：file://<host><path>，主机为空时形如 file:///path
//   - Linux/macOS：abs 已以 / 开头，"file://" + abs 即得 file:///abs（3 个斜杠）
//   - Windows：abs 形如 C:/...，需补一个 / → file:///C:/...
static QByteArray pathToFileUri(const QString& path)
{
    QString abs = QFileInfo(path).absoluteFilePath();   // 转绝对路径
    abs.replace('\\', '/');                             // 反斜杠 -> 正斜杠
    QByteArray uri = QByteArrayLiteral("file://");
    if (!abs.startsWith('/'))
        uri += '/';                                     // Windows 盘符前补 /
    uri += abs.toUtf8();
    return uri;
}

// ============================================================
//  PlayerWindow
// ============================================================
PlayerWindow::PlayerWindow(const QString &path, const QString &title,
                           QWidget *parent)
    : QWidget(parent),
      m_title(title.isEmpty() ? QFileInfo(path).fileName() : title),
      m_path(path)
{
    setWindowTitle(m_title);
    setStyleSheet("background-color: #000000;");
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setFocusPolicy(Qt::StrongFocus);

    auto *app = qobject_cast<MediaApp*>(parent);
    auto P = [app](int v){ return app ? app->px(v) : v; };
    auto F = [app](int v, bool b=false){
        return app ? app->appFont(v, b) : QFont(FONT_FAMILY, v);
    };

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_nameLabel = new QLabel(m_title, this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setFont(F(15));
    m_nameLabel->setFixedHeight(P(34));
    m_nameLabel->setStyleSheet("color: #dddddd; background: #000000;");
    m_nameLabel->setFocusPolicy(Qt::NoFocus);
    root->addWidget(m_nameLabel);

    m_videoFrame = new QWidget(this);
    m_videoFrame->setStyleSheet("background-color: #000000;");
    m_videoFrame->setFocusPolicy(Qt::NoFocus);
    m_videoFrame->setAttribute(Qt::WA_NativeWindow, true);       // 必须有原生句柄
    m_videoFrame->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    m_videoFrame->setAttribute(Qt::WA_NoSystemBackground, true); // Qt 不擦除，避免和 VLC 抢绘制
    m_videoFrame->setAttribute(Qt::WA_OpaquePaintEvent, true);
    root->addWidget(m_videoFrame, 1);

    m_hintLabel = new QLabel(
        "空格 暂停/继续   ←/→ 快退快进 10s   ↑/↓ 音量   "
        "F 全屏/窗口   M 静音   Esc 退出", this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setFont(F(12));
    m_hintLabel->setFixedHeight(P(28));
    m_hintLabel->setStyleSheet("color: #777777; background: #000000;");
    m_hintLabel->setFocusPolicy(Qt::NoFocus);
    root->addWidget(m_hintLabel);

    // ---- 创建 VLC 实例 ----
    // 去掉 --quiet，让 VLC 内部日志能通过 vlcLogCb 打出来
    const char *args[] = {
        "--no-video-title-show",
        "--no-video-deco",        // 不要给视频加标题栏（关键！）
        "--no-embedded-video",    // 不要嵌入视频组件
    };
    m_inst = libvlc_new(sizeof(args) / sizeof(args[0]), args);

    qDebug() << "[PlayerWindow] libvlc_new ->" << m_inst;
    if (!m_inst) {
        const char *err = libvlc_errmsg();
        qDebug() << "[PlayerWindow] VLC 初始化失败:" << (err ? err : "(no msg)");
        // 在界面上给个提示
        m_nameLabel->setText(QString("VLC 初始化失败: %1")
                             .arg(err ? err : "libvlc_new 返回空"));
        m_nameLabel->setStyleSheet("color: #ff8888; background: #000000;");
        return;
    }

    // 安装 VLC 内部日志回调，树莓派上能看到 VLC 报了什么
    libvlc_log_set(m_inst, vlcLogCb, nullptr);

    m_mp = libvlc_media_player_new(m_inst);

    const QByteArray uri = pathToFileUri(path);
    qDebug().noquote() << "[PlayerWindow] file URI:" << uri;
    m_media = libvlc_media_new_location(m_inst, uri.constData());
    if (!m_media) {
        const char* e = libvlc_errmsg();
        qDebug() << "[PlayerWindow] media 创建失败:" << (e ? e : "(no msg)");
        return;
    }

    libvlc_media_player_set_media(m_mp, m_media);

    libvlc_event_manager_t *em = libvlc_media_player_event_manager(m_mp);
    libvlc_event_attach(em, libvlc_MediaPlayerEndReached, onVlcEnded, this);
    libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError, onVlcError, this);

    // 注意：set_hwnd 放到 showEvent 里
}

PlayerWindow::~PlayerWindow()
{
    if (m_mp)    { libvlc_media_player_stop(m_mp); libvlc_media_player_release(m_mp); }
    if (m_media) { libvlc_media_release(m_media); }
    if (m_inst)  { libvlc_release(m_inst); }
}

void PlayerWindow::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);

    // 窗口刚显示，overlay 先亮几秒给用户看到，再自动隐藏
    if (m_fullscreen)
        QTimer::singleShot(2500, this, &PlayerWindow::hideOverlay);

    if (m_attached || !m_mp || !m_media) return;
    m_attached = true;

    // 关键：show 之后 native handle 才真正可用
    m_videoFrame->winId();               // 强制创建

    attachToVlc();                        // 绑定 hwnd + play
}

void PlayerWindow::attachToVlc()
{
#ifdef Q_OS_WIN
    WId wid = m_videoFrame->winId();
    qDebug() << "[PlayerWindow] set_hwnd" << (void*)wid;
    libvlc_media_player_set_hwnd(m_mp, (void*)wid);
#elif defined(Q_OS_LINUX)
    libvlc_media_player_set_xwindow(m_mp, (uint32_t)m_videoFrame->winId());
#elif defined(Q_OS_MAC)
    libvlc_media_player_set_nsobject(m_mp, (void*)m_videoFrame->winId());
#endif

    // 稍微延迟，确保窗口已完全映射
    QTimer::singleShot(100, this, [this](){
        if (!m_mp || !m_media) return;   // m_media 为空时无任何东西可播
        qDebug() << "[PlayerWindow] media:" << m_media;
        qDebug() << "[PlayerWindow] mp:" << m_mp;

        int rc = libvlc_media_player_play(m_mp);
        qDebug() << "[PlayerWindow] play() ->" << rc;

        // play 之后马上查状态
        if (rc == -1) {
            const char *err = libvlc_errmsg();
            qDebug() << "[PlayerWindow] play error:" << (err ? err : "(no msg)");
            qDebug() << "[PlayerWindow] media state:"
                     << libvlc_media_get_state(m_media);
        }
    });
}

void PlayerWindow::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    // m_videoFrame 由布局自动撑满，一般不需要额外处理
}

void PlayerWindow::close()
{
    if (m_closed) return;
    m_closed = true;
    if (m_mp) libvlc_media_player_stop(m_mp);
    emit finished();
    QWidget::close();
}

// ---------- 键盘 ----------
void PlayerWindow::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Space:  togglePause(); break;
    case Qt::Key_Left:   seek(-10000); break;
    case Qt::Key_Right:  seek(10000);  break;
    case Qt::Key_Up:     volume(5);    break;
    case Qt::Key_Down:   volume(-5);   break;
    case Qt::Key_F:      toggleFullscreen(); break;
    case Qt::Key_M:      toggleMute(); break;
    case Qt::Key_Escape: close(); return;
    default:
        QWidget::keyPressEvent(e);
        return;
    }
    e->accept();
}

void PlayerWindow::mouseMoveEvent(QMouseEvent *e)
{
    showOverlay();
    QWidget::mouseMoveEvent(e);
}

// ---------- Overlay ----------
void PlayerWindow::showOverlay()
{
    if (m_closed || !m_fullscreen) return;
    m_nameLabel->show();
    m_hintLabel->show();
    QTimer::singleShot(2500, this, &PlayerWindow::hideOverlay);
}

void PlayerWindow::hideOverlay()
{
    if (m_closed || !m_fullscreen) return;
    m_nameLabel->hide();
    m_hintLabel->hide();
}

// ---------- 播放控制 ----------
void PlayerWindow::togglePause()
{
    if (m_mp) libvlc_media_player_pause(m_mp);
}

void PlayerWindow::seek(int ms)
{
    if (!m_mp) return;
    libvlc_time_t cur = libvlc_media_player_get_time(m_mp);
    libvlc_media_player_set_time(m_mp, qMax<libvlc_time_t>(0, cur + ms));
}

void PlayerWindow::volume(int delta)
{
    if (!m_mp) return;
    int cur = libvlc_audio_get_volume(m_mp);
    libvlc_audio_set_volume(m_mp, qBound(0, cur + delta, 150));
}

void PlayerWindow::toggleMute()
{
    if (m_mp) libvlc_audio_toggle_mute(m_mp);
}

void PlayerWindow::toggleFullscreen()
{
    m_fullscreen = !m_fullscreen;
    if (m_fullscreen) {
        setWindowState(Qt::WindowFullScreen);
        show();
        QTimer::singleShot(2500, this, &PlayerWindow::hideOverlay);
    } else {
        setWindowState(Qt::WindowNoState);
        show();
        m_nameLabel->show();
        m_hintLabel->show();
    }
}

// ---------- VLC 事件 ----------
void PlayerWindow::handleEnded()
{
    if (!m_closed) close();
}

void PlayerWindow::handleError()
{
    if (m_mp) {
        const char *err = libvlc_errmsg();
        qDebug() << "[PlayerWindow] VLC error:" << (err ? err : "(no msg)");
    }
    auto *app = qobject_cast<MediaApp*>(parent());
    if (app) app->setFocus();
    if (!m_closed) close();
}

// ============================================================
//  PictureViewer
// ============================================================
PictureViewer::PictureViewer(const QVector<MediaItem> &pics, int index,
                             QWidget *parent)
    : QWidget(parent), m_pics(pics), m_index(index)
{
    setWindowTitle("图片查看");
    setStyleSheet("background-color: #000000;");
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setFocusPolicy(Qt::StrongFocus);

    auto *app = qobject_cast<MediaApp*>(parent);
    auto P = [app](int v){ return app ? app->px(v) : v; };
    auto F = [app](int v, bool b=false){
        return app ? app->appFont(v, b) : QFont(FONT_FAMILY, v);
    };

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setStyleSheet("background-color: #000000;");
    m_label->setFocusPolicy(Qt::NoFocus);
    root->addWidget(m_label, 1);

    m_info = new QLabel(this);
    m_info->setAlignment(Qt::AlignCenter);
    m_info->setFont(F(14));
    m_info->setFixedHeight(P(34));
    m_info->setStyleSheet("color: #999999; background: #000000;");
    m_info->setFocusPolicy(Qt::NoFocus);
    root->addWidget(m_info);
}

void PictureViewer::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Left:  case Qt::Key_Up:
        prev(); break;
    case Qt::Key_Right: case Qt::Key_Down:
    case Qt::Key_Return: case Qt::Key_Enter:
    case Qt::Key_Space:
        next(); break;
    case Qt::Key_Escape:
        close(); return;
    default:
        QWidget::keyPressEvent(e);
        return;
    }
    e->accept();
}

void PictureViewer::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    showCurrent();
}

void PictureViewer::prev()
{
    if (m_pics.isEmpty()) return;
    m_index = (m_index - 1 + m_pics.size()) % m_pics.size();
    showCurrent();
}

void PictureViewer::next()
{
    if (m_pics.isEmpty()) return;
    m_index = (m_index + 1) % m_pics.size();
    showCurrent();
}

void PictureViewer::showCurrent()
{
    if (m_pics.isEmpty()) return;
    const MediaItem &item = m_pics[m_index];

    m_source = loadPixmapFile(item.path);
    if (m_source.isNull()) {
        m_label->setPixmap(QPixmap());
        m_label->setText("无法打开图片");
        m_label->setStyleSheet("color: #ff8888; background: #000000;");
    } else {
        const int w = qMax(100, width()  - 40);
        const int h = qMax(100, height() - 90);
        const QPixmap scaled = m_source.scaled(w, h, Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation);
        m_label->setText("");
        m_label->setStyleSheet("background-color: #000000;");
        m_label->setPixmap(scaled);
    }

    m_info->setText(QString("%1 / %2    %3    ← → 切换   Esc 返回")
                    .arg(m_index + 1).arg(m_pics.size())
                    .arg(item.disp.isEmpty() ? item.name : item.disp));
}

void PictureViewer::close()
{
    QWidget::close();
}