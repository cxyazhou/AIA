#pragma once
#include <QtWidgets/QDialog>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "common.h"

class QListWidget;
class QLabel;
class QTimer;
class QKeyEvent;
class QMouseEvent;
class MediaApp;

class QListWidget;
class QLabel;
class QTimer;
class QKeyEvent;
class MediaApp;

// ============================================================
// 电视剧选集
// ============================================================
class EpisodeDialog : public QDialog {
    Q_OBJECT
public:
    EpisodeDialog(QWidget *parent, const MediaItem &item,
                  const QStringList &episodes);

    QString resultPath()  const { return m_resultPath; }
    QString resultTitle() const { return m_resultTitle; }

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private slots:
    void onAccept();

private:
    MediaItem        m_item;
    QStringList      m_episodes;
    QListWidget     *m_list  = nullptr;
    QString          m_resultPath;
    QString          m_resultTitle;
    QPoint           m_dragOffset;
    bool             m_dragging = false;
};

// ============================================================
// 退出 / 关机倒计时
// ============================================================
class CountdownDialog : public QDialog {
    Q_OBJECT
public:
    CountdownDialog(MediaApp *app, const QString &actionKey,
                    int seconds = COUNTDOWN_SECONDS);

protected:
    void keyPressEvent(QKeyEvent *e) override;

private slots:
    void tick();
    void executeNow();
    void cancel();

private:
    void render();
    void closeMe();

    MediaApp *m_app       = nullptr;
    QString   m_actionKey;
    int       m_remaining = 0;
    bool      m_finished  = false;
    QLabel   *m_numLabel  = nullptr;
    QTimer   *m_timer     = nullptr;
};