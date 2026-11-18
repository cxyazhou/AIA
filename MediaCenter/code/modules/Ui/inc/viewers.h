#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QPixmap>
#include <QtCore/QString>
#include <QtCore/QVector>

#include "common.h"

class QLabel;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QShowEvent;

struct libvlc_instance_t;
struct libvlc_media_player_t;
struct libvlc_media_t;

// ============================================================
// VLC 播放窗口
// ============================================================
class PlayerWindow : public QWidget {
    Q_OBJECT
public:
    PlayerWindow(const QString &path, const QString &title,
                 QWidget *parent = nullptr);
    ~PlayerWindow();

    void close();

    // VLC 事件回调（公开，供 QTimer::singleShot lambda 调用）
    void handleEnded();
    void handleError();

signals:
    void finished();

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void showEvent(QShowEvent *e) override;      // ← 在 show 之后绑 hwnd + play
    void resizeEvent(QResizeEvent *e) override;  // ← 窗口尺寸变化时，让视频区跟随

private slots:
    void hideOverlay();

private:
    void showOverlay();
    void togglePause();
    void seek(int ms);
    void volume(int delta);
    void toggleMute();
    void toggleFullscreen();
    void attachToVlc();   // ← 抽出来，在 showEvent 里调用

    QString m_title;
    QString m_path;
    bool    m_closed = false;
    bool    m_fullscreen = true;
    bool    m_attached = false;   // 防止重复 attach + play

    QLabel  *m_nameLabel  = nullptr;
    QWidget *m_videoFrame = nullptr;
    QLabel  *m_hintLabel  = nullptr;

    libvlc_instance_t     *m_inst  = nullptr;
    libvlc_media_player_t *m_mp    = nullptr;
    libvlc_media_t        *m_media = nullptr;
};

// ============================================================
// 图片查看
// ============================================================
class PictureViewer : public QWidget {
    Q_OBJECT
public:
    PictureViewer(const QVector<MediaItem> &pics, int index,
                  QWidget *parent = nullptr);

    void close();

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    void prev();
    void next();
    void showCurrent();

    QVector<MediaItem> m_pics;
    int      m_index = 0;
    QLabel  *m_label = nullptr;
    QLabel  *m_info  = nullptr;
    QPixmap  m_source;
};