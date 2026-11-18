#include "dialogs.h"
#include "mediaapp.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFrame>

#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QScreen>

#include <QtCore/QTimer>
#include <QtCore/QFileInfo>

// ============================================================
//  EpisodeDialog
// ============================================================
EpisodeDialog::EpisodeDialog(QWidget *parent, const MediaItem &item,
                             const QStringList &episodes)
    : QDialog(parent), m_item(item), m_episodes(episodes)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    setStyleSheet(QString(
        "QDialog { background-color: %1; border: 2px solid %2; }")
        .arg(C::PANEL, C::ACCENT));

    auto *app = qobject_cast<MediaApp*>(parent);
    auto P = [app](int v){ return app ? app->px(v) : v; };
    auto F = [app](int v, bool b=false){
        return app ? app->appFont(v, b) : QFont(FONT_FAMILY, v);
    };

    const int w = P(720), h = P(580);
    if (parent) {
        const QRect pg = parent->geometry();
        move(pg.x() + (pg.width() - w) / 2, pg.y() + (pg.height() - h) / 2);
    }
    resize(w, h);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // 标题栏
    auto *header = new QWidget();
    header->setStyleSheet("background: transparent;");
    auto *hl = new QHBoxLayout(header);
    hl->setContentsMargins(P(24), P(10), 0, P(10));

    auto *titleLb = new QLabel(QString("《%1》 选择剧集").arg(item.name));
    titleLb->setFont(F(20, true));
    titleLb->setStyleSheet(QString("color: %1; background: transparent;").arg(C::TEXT));
    titleLb->setFocusPolicy(Qt::NoFocus);
    hl->addWidget(titleLb);
    hl->addStretch(1);

    auto *closeBtn = new QPushButton("  ✕  ");
    closeBtn->setFont(F(16));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setFixedWidth(P(48));
    closeBtn->setStyleSheet(QString(
        "QPushButton { color: %1; background: transparent; border: none; }"
        "QPushButton:hover { background-color: %2; color: #ffffff; }")
        .arg(C::SUB, C::DANGER));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    hl->addWidget(closeBtn);

    root->addWidget(header);

    auto *line = new QFrame();
    line->setFixedHeight(1);
    line->setStyleSheet("background-color: #262626; border: none;");
    root->addWidget(line);

    // 列表
    auto *body = new QWidget();
    body->setStyleSheet("background: transparent;");
    auto *bl = new QVBoxLayout(body);
    bl->setContentsMargins(P(18), P(12), P(18), 0);

    m_list = new QListWidget(body);
    m_list->setFont(F(16));
    m_list->setFocusPolicy(Qt::StrongFocus);
    m_list->setStyleSheet(QString(
        "QListWidget {"
        "  background-color: #151515; color: %1; border: none; outline: none;"
        "  padding: 4px;"
        "}"
        "QListWidget::item { padding: 6px 10px; border: none; }"
        "QListWidget::item:selected { background-color: %2; color: #ffffff; }")
        .arg(C::TEXT, C::ACCENT));

    for (const QString &name : episodes)
        m_list->addItem("  " + QFileInfo(name).completeBaseName());
    m_list->setCurrentRow(0);
    bl->addWidget(m_list, 1);
    root->addWidget(body, 1);

    auto *tip = new QLabel("↑↓ 选择   Enter / 双击 播放   Esc 返回");
    tip->setFont(F(12));
    tip->setStyleSheet(QString("color: %1; background: transparent;").arg(C::SUB));
    tip->setContentsMargins(P(24), P(8), 0, P(10));
    tip->setFocusPolicy(Qt::NoFocus);
    root->addWidget(tip);

    connect(m_list, &QListWidget::itemDoubleClicked,
            this, &EpisodeDialog::onAccept);
    connect(m_list, &QListWidget::itemActivated,
            this, &EpisodeDialog::onAccept);

    // 列表拿到焦点后，键盘 Enter 直接触发 onAccept（itemActivated 已连接）
    QTimer::singleShot(60, m_list, [this](){ m_list->setFocus(); });
}

void EpisodeDialog::onAccept()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_episodes.size()) return;
    const QString full = m_episodes[row];
    m_resultPath  = m_item.path + "/" + full;
    m_resultTitle = QString("%1 - %2").arg(m_item.name,
                                           QFileInfo(full).completeBaseName());
    accept();
}

void EpisodeDialog::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && e->pos().y() < 60) {
        m_dragging = true;
        m_dragOffset = e->globalPos() - frameGeometry().topLeft();
        e->accept();
    }
}

void EpisodeDialog::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPos() - m_dragOffset);
        e->accept();
    }
}

void EpisodeDialog::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
}

// ============================================================
//  CountdownDialog
// ============================================================
CountdownDialog::CountdownDialog(MediaApp *app, const QString &actionKey, int seconds)
    : QDialog(app), m_app(app), m_actionKey(actionKey), m_remaining(seconds)
{
    const bool isShutdown = (actionKey == "shutdown");
    const QString titleText = isShutdown ? "关闭电脑" : "退出程序";
    const QString detail = isShutdown
        ? "电脑将在倒计时结束后关闭，请确认没有未保存的工作。"
        : "媒体中心将在倒计时结束后关闭。";

    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);
    setModal(true);
    setWindowTitle("确认" + titleText);
    setStyleSheet(QString("QDialog { background-color: %1; border: 2px solid %2; }")
                  .arg(C::PANEL, C::DANGER));

    auto P = [app](int v){ return app->px(v); };
    auto F = [app](int v, bool b=false){ return app->appFont(v, b); };

    const int w = P(540), h = P(320);
    const QRect pg = app->geometry();
    move(pg.x() + (pg.width() - w) / 2, pg.y() + (pg.height() - h) / 2);
    resize(w, h);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, P(20), 0, P(12));
    root->setSpacing(P(6));

    auto *t = new QLabel(titleText, this);
    t->setAlignment(Qt::AlignCenter);
    t->setFont(F(28, true));
    t->setStyleSheet(QString("color: %1; background: transparent;").arg(C::DANGER));
    t->setFocusPolicy(Qt::NoFocus);
    root->addWidget(t);

    m_numLabel = new QLabel(this);
    m_numLabel->setAlignment(Qt::AlignCenter);
    m_numLabel->setFont(F(40, true));
    m_numLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(C::TEXT));
    m_numLabel->setFocusPolicy(Qt::NoFocus);
    root->addWidget(m_numLabel);

    auto *d = new QLabel(detail, this);
    d->setAlignment(Qt::AlignCenter);
    d->setWordWrap(true);
    d->setFont(F(14));
    d->setStyleSheet(QString("color: %1; background: transparent;").arg(C::SUB));
    d->setFocusPolicy(Qt::NoFocus);
    root->addWidget(d);

    root->addStretch(1);

    auto *btns = new QWidget(this);
    auto *bl = new QHBoxLayout(btns);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(P(20));

    auto *ok = new QPushButton("  立即执行  ", btns);
    ok->setFont(F(16, true));
    ok->setCursor(Qt::PointingHandCursor);
    ok->setFocusPolicy(Qt::NoFocus);
    ok->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ok->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: #ffffff;"
        "  border: none; padding: 10px 18px; }"
        "QPushButton:hover { background-color: #ff5f4d; }").arg(C::DANGER));
    connect(ok, &QPushButton::clicked, this, &CountdownDialog::executeNow);

    auto *cancel = new QPushButton("  取消  ", btns);
    cancel->setFont(F(16));
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setFocusPolicy(Qt::NoFocus);
    cancel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    cancel->setStyleSheet(QString(
        "QPushButton { background-color: #2a2a2a; color: %1;"
        "  border: none; padding: 10px 18px; }"
        "QPushButton:hover { background-color: #3a3a3a; }").arg(C::TEXT));
    connect(cancel, &QPushButton::clicked, this, &CountdownDialog::cancel);

    bl->addWidget(ok);
    bl->addWidget(cancel);
    root->addWidget(btns);

    auto *tip = new QLabel("Enter 立即执行    Esc 取消", this);
    tip->setAlignment(Qt::AlignCenter);
    tip->setFont(F(12));
    tip->setStyleSheet("color: #666666; background: transparent;");
    tip->setFocusPolicy(Qt::NoFocus);
    root->addWidget(tip);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &CountdownDialog::tick);

    render();
    m_timer->start();
}

void CountdownDialog::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        executeNow(); break;
    case Qt::Key_Escape:
        cancel(); break;
    default:
        e->accept();
    }
}

void CountdownDialog::render()
{
    m_numLabel->setText(QString("%1 秒后自动%2")
        .arg(m_remaining)
        .arg(m_actionKey == "shutdown" ? "关机" : "退出"));
}

void CountdownDialog::tick()
{
    if (m_finished) return;
    --m_remaining;
    if (m_remaining <= 0) { executeNow(); return; }
    render();
}

void CountdownDialog::executeNow()
{
    if (m_finished) return;
    m_finished = true;
    m_timer->stop();
    const QString key = m_actionKey;
    MediaApp *app = m_app;
    closeMe();
    if (app) app->executeAction(key);
}

void CountdownDialog::cancel()
{
    if (m_finished) return;
    m_finished = true;
    m_timer->stop();
    closeMe();
}

void CountdownDialog::closeMe()
{
    if (m_app) m_app->m_confirm = nullptr;   // 需要 friend 或者 public 成员
    close();
    if (m_app) { m_app->raise(); m_app->activateWindow(); m_app->setFocus(); }
}