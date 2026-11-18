#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <QtCore/QHash>
#include <QtCore/QVector>
#include <QtCore/QPair>
#include <QtCore/QString>

#include <functional>

#include "common.h"

class QLabel;
class QFrame;
class QVBoxLayout;
class QTimer;
class QPushButton;
class QResizeEvent;
class QKeyEvent;
class PlayerWindow;
class PictureViewer;
class CountdownDialog;

struct LayoutInfo { int cols = 1, rows = 1, pageSize = 1; };

// ---------- 海报卡片 ----------
class CardWidget : public QFrame {
public:
    CardWidget(int pos, int cellW, int imgH, int titleH, QWidget *parent = nullptr);
    int position() const { return m_pos; }
    void setPixmap(const QPixmap &pix);
    void setTitleText(const QString &text, const QFont &n, const QFont &b, int maxW);
    void setSelected(bool sel);
    std::function<void(int)> onClick;
protected:
    void mousePressEvent(QMouseEvent *e) override;
private:
    int     m_pos;
    int     m_cellW, m_imgH, m_titleH;
    QFrame *m_inner = nullptr;
    QLabel *m_img   = nullptr;
    QLabel *m_title = nullptr;
    bool    m_selected = false;
};

// ---------- 音乐行 ----------
class MusicRow : public QFrame {
public:
    MusicRow(int pos, int idx, int rowH, QWidget *parent = nullptr);
    int position() const { return m_pos; }
    void setContent(const QString &name, const QString &folder,
                    const QFont &n, const QFont &b, const QFont &s,
                    bool selected);
    std::function<void(int)> onClick;
protected:
    void mousePressEvent(QMouseEvent *e) override;
private:
    int     m_pos, m_idx;
    QFrame *m_inner  = nullptr;
    QLabel *m_mark   = nullptr;
    QLabel *m_name   = nullptr;
    QLabel *m_folder = nullptr;
};

// ---------- 主窗口 ----------
class MediaApp : public QWidget {
    Q_OBJECT
    friend class CountdownDialog;
public:
    explicit MediaApp(QWidget *parent = nullptr);
    ~MediaApp();

    int   px(int v) const;
    QFont appFont(int basePx, bool bold = false) const;

    void playMedia(const QString &path, const QString &title);
    void requestAction(const QString &actionKey);
    void executeAction(const QString &actionKey);

    void focusMainWindow();

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private slots:
    void bootstrap();
    void renderSidebar();
    void renderContent();
    void onResizeDone();
    void onMenuClicked(int idx);
    void onCardClicked(int pos);

public slots:
    void rescanLibrary();

private:
    void buildUi();
    void computeScale();
    void clearContent();

    void planGrid(const QString &key, int aw, int ah,
                  int &cols, int &rows, int &cw, int &ih,
                  int &gap, int &th);
    QPair<int,int> contentArea() const;

    void renderCards(const QString &key, const QString &glyph,
                     const QVector<MediaItem> &items,
                     int start, int pageSize,
                     int cols, int cellW, int imgH, int gap, int titleH,
                     int idx);
    void renderMusic(const QVector<MediaItem> &items,
                     int start, int pageSize, int idx);
    void renderActionPage();

    void updateSelectionDisplay(const QString &key, int oldPos, int newPos);
    void refreshRightSelection(const QString &key);
    void moveSelectionVisual(int delta);
    int  stepVertical(const QString &key) const;

    void activate(const MediaItem &item);
    void openEpisodeDialog(const MediaItem &item);
    void openPictureViewer(const MediaItem &item);
    void openExternal(const QString &path);

    void refreshAll();
    void onUp();
    void onDown();
    void onLeft();
    void onRight();
    void onEnter();
    void onEscape();

    // 数据
    QHash<QString, QVector<MediaItem>> m_items;
    QHash<QString, int>                m_sel;
    QHash<QString, LayoutInfo>         m_layout;
    int     m_menuIndex = 0;
    QString m_zone = "left";

    ThumbCache *m_cache = nullptr;
    double      m_scale = 1.0;
    QHash<QString, int> m_metrics;

    // UI
    QFrame      *m_side        = nullptr;
    QVector<QPushButton*> m_sideButtons;
    QLabel      *m_title       = nullptr;
    QLabel      *m_page        = nullptr;
    QLabel      *m_hint        = nullptr;
    QLabel      *m_status      = nullptr;
    QWidget     *m_contentHost = nullptr;
    QVBoxLayout *m_contentBox  = nullptr;

    QHash<int, CardWidget*> m_cardWidgets;
    QHash<int, MusicRow*>   m_musicRows;

    // 子窗口
    PlayerWindow    *m_player  = nullptr;
    PictureViewer   *m_viewer  = nullptr;
    CountdownDialog *m_confirm = nullptr;
    bool             m_exiting = false;

    QTimer *m_resizeTimer = nullptr;
};