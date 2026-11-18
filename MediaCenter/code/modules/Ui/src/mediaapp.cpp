#include "mediaapp.h"
#include "dialogs.h"
#include "viewers.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QFrame>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QDialog>

#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QScreen>
#include <QtGui/QFontMetrics>

#include <QtCore/QTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QRegularExpression>

// ============================================================
//  CardWidget
// ============================================================
CardWidget::CardWidget(int pos, int cellW, int imgH, int titleH, QWidget *parent)
    : QFrame(parent), m_pos(pos),
      m_cellW(cellW), m_imgH(imgH), m_titleH(titleH)
{
    setFixedSize(cellW, imgH + titleH);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("cardOuter");

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(3, 3, 3, 3);
    outer->setSpacing(0);

    m_inner = new QFrame(this);
    m_inner->setObjectName("cardInner");
    m_inner->setAttribute(Qt::WA_StyledBackground, true);
    outer->addWidget(m_inner);

    auto *lay = new QVBoxLayout(m_inner);
    lay->setContentsMargins(5, 5, 5, 3);
    lay->setSpacing(3);

    m_img = new QLabel(m_inner);
    m_img->setAlignment(Qt::AlignCenter);
    m_img->setFixedSize(qMax(1, cellW - 16), qMax(1, imgH - 10));
    m_img->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    lay->addWidget(m_img, 0, Qt::AlignHCenter);

    m_title = new QLabel(m_inner);
    m_title->setAlignment(Qt::AlignCenter);
    m_title->setFixedHeight(qMax(1, titleH - 8));
    m_title->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    lay->addWidget(m_title);

    setSelected(false);
}

void CardWidget::setPixmap(const QPixmap &pix)
{
    m_img->setPixmap(pix);
}

void CardWidget::setTitleText(const QString &text, const QFont &n, const QFont &b,
                              int maxW)
{
    QFontMetrics fm(m_selected ? b : n);
    m_title->setText(fm.elidedText(text, Qt::ElideRight, maxW));
    m_title->setFont(m_selected ? b : n);
}

void CardWidget::setSelected(bool sel)
{
    m_selected = sel;
    const QString border = sel ? C::CARD_SEL : QStringLiteral("#0c0c0c");
    setStyleSheet(QString(
        "QFrame#cardOuter { background-color: %1; border: none; }"
        "QFrame#cardInner { background-color: %2; border: none; }")
        .arg(border, C::CARD));
    m_title->setStyleSheet(QString("color: %1; background: transparent;")
                           .arg(sel ? C::TEXT : QStringLiteral("#d8d8d8")));
}

void CardWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && onClick) onClick(m_pos);
    QFrame::mousePressEvent(e);
}

// ============================================================
//  MusicRow
// ============================================================
MusicRow::MusicRow(int pos, int idx, int rowH, QWidget *parent)
    : QFrame(parent), m_pos(pos), m_idx(idx)
{
    setFixedHeight(rowH);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("musicRow");

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_inner = new QFrame(this);
    m_inner->setObjectName("musicInner");
    m_inner->setAttribute(Qt::WA_StyledBackground, true);
    outer->addWidget(m_inner);

    auto *lay = new QHBoxLayout(m_inner);
    lay->setContentsMargins(12, 0, 18, 0);
    lay->setSpacing(6);

    m_mark = new QLabel(m_inner);
    m_mark->setFixedWidth(60);
    m_mark->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    m_name = new QLabel(m_inner);
    m_name->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    m_folder = new QLabel(m_inner);
    m_folder->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_folder->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    lay->addWidget(m_mark);
    lay->addWidget(m_name, 1);
    lay->addWidget(m_folder);
}

void MusicRow::setContent(const QString &name, const QString &folder,
                          const QFont &n, const QFont &b, const QFont &s,
                          bool selected)
{
    const QString bg      = selected ? C::SEL_BG  : C::CARD;
    const QString fgName  = selected ? "#ffffff"  : C::TEXT;
    const QString fgMark  = selected ? "#ffffff"  : C::SUB;
    const QString fgDir   = selected ? "#bfe0f7"  : C::SUB;

    setStyleSheet(QString("QFrame#musicInner { background-color: %1; border: none; }").arg(bg));

    m_mark->setText(QString(" %1 %2").arg(selected ? "▶" : "·")
                    .arg(m_idx, 2, 10, QChar('0')));
    m_mark->setFont(b);
    m_mark->setStyleSheet(QString("color: %1; background: transparent;").arg(fgMark));

    m_name->setText(name);
    m_name->setFont(selected ? b : n);
    m_name->setStyleSheet(QString("color: %1; background: transparent;").arg(fgName));

    m_folder->setText(folder);
    m_folder->setFont(s);
    m_folder->setStyleSheet(QString("color: %1; background: transparent;").arg(fgDir));
}

void MusicRow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && onClick) onClick(m_pos);
    QFrame::mousePressEvent(e);
}

// ============================================================
//  MediaApp
// ============================================================
static const QHash<QString,int> s_baseMetrics = {
    {"side_w", 240}, {"side_title", 30}, {"side_item", 22}, {"side_tip", 12},
    {"top_title", 32}, {"top_page", 16}, {"hint", 14}, {"status", 13},
    {"card_title", 15}, {"music_item", 17}, {"grid_gap", 10},
    {"card_title_h", 30}, {"music_row_h", 50},
};

MediaApp::MediaApp(QWidget *parent)
    : QWidget(parent)
{
    m_cache = new ThumbCache(400);

    computeScale();

    setWindowTitle("媒体中心");
    setStyleSheet(QString("QWidget { background-color: %1; }").arg(C::BG));
    setFocusPolicy(Qt::StrongFocus);

    buildUi();

    // 全屏
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setGeometry(QApplication::primaryScreen()->availableGeometry());
    showFullScreen();
    raise();
    activateWindow();
    setFocus();

    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true);
    connect(m_resizeTimer, &QTimer::timeout, this, &MediaApp::onResizeDone);

    QTimer::singleShot(60, this, &MediaApp::bootstrap);
}

MediaApp::~MediaApp()
{
    delete m_cache;
}

int MediaApp::px(int v) const
{
    return qMax(1, int(qRound(v * m_scale)));
}

QFont MediaApp::appFont(int basePx, bool bold) const
{
    QFont f(FONT_FAMILY);
    f.setPixelSize(px(basePx));
    f.setBold(bold);
    return f;
}

void MediaApp::computeScale()
{
    const QRect geo = QApplication::primaryScreen()->availableGeometry();
    m_scale = qMax(1.0, qMin(2.0, qMin(geo.width() / 1920.0, geo.height() / 1080.0)));
    m_metrics.clear();
    for (auto it = s_baseMetrics.constBegin(); it != s_baseMetrics.constEnd(); ++it)
        m_metrics.insert(it.key(), px(it.value()));
}

void MediaApp::rescanLibrary()
{
    QStringList counts;
    for (const Category& cat : CATEGORIES) {
        const QString path = baseDir() + "/" + cat.fileName;
        QVector<MediaItem> items;
        if (QFile::exists(path)) {
            const QStringList paths = readListFile(path);
            if (cat.key == "tv")         items = scanTv(paths);
            else if (cat.key == "movie") items = scanFiles(paths, VIDEO_EXTS, "movie");
            else if (cat.key == "pic")   items = scanFiles(paths, IMAGE_EXTS, "pic");
            else                         items = scanFiles(paths, AUDIO_EXTS, "music");
        }
        m_items[cat.key] = items;
        m_sel[cat.key] = items.isEmpty() ? 0
                        : qMin(m_sel.value(cat.key), items.size() - 1);
        counts << QString("%1 %2 项").arg(cat.name).arg(items.size());
    }
    m_cache->clear();   // 缩略图缓存清掉，否则新图片显示不出来
    m_status->setText(counts.join("   "));
    renderSidebar();
    renderContent();
}

// ---------------- UI ----------------
void MediaApp::buildUi()
{
    const auto M = [this](const char *k){ return m_metrics.value(k); };

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---------- 左侧 ----------
    m_side = new QFrame(this);
    m_side->setFixedWidth(M("side_w"));
    m_side->setStyleSheet(QString("QFrame { background-color: %1; border: none; }").arg(C::PANEL));
    auto *sideLay = new QVBoxLayout(m_side);
    sideLay->setContentsMargins(0, 0, 0, 0);
    sideLay->setSpacing(0);

    auto *sideTitle = new QLabel("媒体中心", m_side);
    sideTitle->setAlignment(Qt::AlignCenter);
    sideTitle->setFont(appFont(30, true));
    sideTitle->setFixedHeight(px(80));
    sideTitle->setFocusPolicy(Qt::NoFocus);
    sideTitle->setStyleSheet(QString("color: %1; background: transparent;").arg(C::ACCENT));
    sideLay->addWidget(sideTitle);

    for (int i = 0; i < N_MENU; ++i) {
        if (i == N_CAT) {
            auto *wrap = new QWidget(m_side);
            wrap->setFixedHeight(px(29));
            wrap->setFocusPolicy(Qt::NoFocus);
            auto *sl = new QVBoxLayout(wrap);
            sl->setContentsMargins(px(24), px(14), px(24), px(14));
            auto *line = new QFrame(wrap);
            line->setFixedHeight(1);
            line->setStyleSheet("background-color: #262626; border: none;");
            sl->addWidget(line);
            sideLay->addWidget(wrap);
        }

        const QString label = (i < N_CAT) ? CATEGORIES[i].name
                                          : ACTIONS[i - N_CAT].name;
        auto *btn = new QPushButton("  " + label, m_side);
        btn->setFixedHeight(px(52));
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this,
                [this, i](){ onMenuClicked(i); });
        sideLay->addWidget(btn);
        m_sideButtons.append(btn);
    }
    sideLay->addStretch(1);

    auto *tip = new QLabel("↑↓ 切换   → 浏览   Enter 播放/执行", m_side);
    tip->setAlignment(Qt::AlignCenter);
    tip->setFont(appFont(12));
    tip->setFixedHeight(px(46));
    tip->setFocusPolicy(Qt::NoFocus);
    tip->setStyleSheet("color: #555555; background: transparent;");
    sideLay->addWidget(tip);

    root->addWidget(m_side);

    // ---------- 右侧 ----------
    auto *right = new QWidget(this);
    right->setStyleSheet(QString("background-color: %1;").arg(C::BG));
    auto *rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(0);

    // 顶部标题行
    auto *topbar = new QWidget(right);
    auto *topLay = new QHBoxLayout(topbar);
    topLay->setContentsMargins(px(40), px(16), px(40), 0);
    topLay->setSpacing(8);

    m_title = new QLabel("正在扫描媒体库…", topbar);
    m_title->setFont(appFont(32, true));
    m_title->setStyleSheet(QString("color: %1; background: transparent;").arg(C::TEXT));
    m_title->setFocusPolicy(Qt::NoFocus);
    topLay->addWidget(m_title);
    topLay->addStretch(1);

    m_page = new QLabel("", topbar);
    m_page->setFont(appFont(16));
    m_page->setStyleSheet(QString("color: %1; background: transparent;").arg(C::SUB));
    m_page->setFocusPolicy(Qt::NoFocus);
    topLay->addWidget(m_page);
    rightLay->addWidget(topbar);

    m_hint = new QLabel("方向键移动选择   Enter 播放 / 选集   Esc 返回", right);
    m_hint->setFont(appFont(14));
    m_hint->setStyleSheet("color: #666666; background: transparent;");
    m_hint->setContentsMargins(px(40), px(8), 0, 0);
    m_hint->setFocusPolicy(Qt::NoFocus);
    rightLay->addWidget(m_hint);

    m_contentHost = new QWidget(right);
    m_contentHost->setStyleSheet(QString("background-color: %1;").arg(C::BG));
    m_contentHost->setFocusPolicy(Qt::NoFocus);
    m_contentBox = new QVBoxLayout(m_contentHost);
    m_contentBox->setContentsMargins(px(20), px(10), px(20), px(10));
    m_contentBox->setSpacing(0);
    rightLay->addWidget(m_contentHost, 1);

    m_status = new QLabel("", right);
    m_status->setFont(appFont(13));
    m_status->setStyleSheet("color: #555555; background: transparent;");
    m_status->setContentsMargins(px(40), 0, px(40), px(10));
    m_status->setFocusPolicy(Qt::NoFocus);
    rightLay->addWidget(m_status);

    root->addWidget(right, 1);
}

// ---------------- 尺寸变化 ----------------
void MediaApp::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_resizeTimer) m_resizeTimer->start(250);
}

void MediaApp::onResizeDone()
{
    if (m_player || m_viewer || m_confirm) return;
    renderContent();
}

// ---------------- 启动扫描 ----------------
void MediaApp::bootstrap()
{
    // 启动时的扫描逻辑与 F5 重新扫描完全一致：m_cache 此时是空的，
    // m_sel 各键也未设置（QHash::value 默认 0），结果与逐项置 0 等价。
    rescanLibrary();
    setFocus();
}

// ---------------- 侧栏渲染 ----------------
void MediaApp::renderSidebar()
{
    for (int i = 0; i < m_sideButtons.size(); ++i) {
        QPushButton *btn = m_sideButtons[i];
        const bool isAction = i >= N_CAT;
        const QString baseFg = isAction ? C::DANGER : C::SUB;

        QString bg = C::PANEL, fg = baseFg;
        bool bold = false;

        if (i == m_menuIndex && m_zone == "left") {
            bg = isAction ? C::DANGER_BG : C::SEL_BG;
            fg = isAction ? C::DANGER    : C::ACCENT;
            bold = true;
        } else if (i == m_menuIndex) {
            bg = "#101418"; fg = C::TEXT; bold = true;
        }

        btn->setFont(appFont(22, bold));
        btn->setStyleSheet(QString(
            "QPushButton {"
            "   text-align: left; padding-left: %1px;"
            "   background-color: %2; color: %3; border: none;"
            "}"
            "QPushButton:hover { background-color: %2; }")
            .arg(px(26)).arg(bg, fg));
    }
}

// ---------------- 内容渲染 ----------------
void MediaApp::clearContent()
{
    while (m_contentBox->count()) {
        QLayoutItem *it = m_contentBox->takeAt(0);
        if (QWidget *w = it->widget()) { w->setParent(nullptr); w->deleteLater(); }
        delete it;
    }
    m_cardWidgets.clear();
    m_musicRows.clear();
}

QPair<int,int> MediaApp::contentArea() const
{
    const int w = qMax(400, width() - m_metrics.value("side_w") - px(60));
    const int h = qMax(300, height() - px(200));
    return { w, h };
}

void MediaApp::planGrid(const QString &key, int areaW, int areaH,
                        int &cols, int &rows, int &cellW, int &imgH,
                        int &gap, int &titleH)
{
    gap    = m_metrics.value("grid_gap");
    titleH = m_metrics.value("card_title_h");

    const double ratio  = (key == "pic") ? 1.60 : 0.70;
    const int    target = px(key == "pic" ? 250 : 180);
    const int    minW   = px(105);

    if (areaW < 50 || areaH < 50) {
        cols = 5; rows = 2; cellW = px(170); imgH = px(240);
        return;
    }

    cols = int(qRound(areaW / double(target + gap)));
    cols = qBound(2, cols, 24);
    while (cols > 1) {
        const double cw = (areaW - gap * (cols + 1)) / double(cols);
        if (cw >= minW) break;
        --cols;
    }

    const double cw = qMax(double(minW),
                           (areaW - gap * (cols + 1)) / double(cols));
    imgH = int(cw / ratio);
    const double cellH = imgH + titleH;
    rows = qMax(1, int((areaH - gap) / (cellH + gap)));
    cellW = int(cw);
}

void MediaApp::renderContent()
{
    const int catIdx = m_menuIndex;
    if (catIdx >= N_CAT) { renderActionPage(); return; }

    const Category &cat = CATEGORIES[catIdx];
    const QVector<MediaItem> &items = m_items.value(cat.key);
    const QString &key = cat.key;

    clearContent();
    m_title->setText(cat.name);

    if (items.isEmpty()) {
        m_page->setText("");
        auto *empty = new QLabel("（暂无内容，请检查对应清单文件中的路径）");
        empty->setAlignment(Qt::AlignCenter);
        empty->setFont(appFont(18));
        empty->setStyleSheet(QString("color: %1; background: transparent;").arg(C::SUB));
        empty->setFocusPolicy(Qt::NoFocus);
        m_contentBox->addWidget(empty, 1);
        m_layout[key] = LayoutInfo{1, 1, 1};
        return;
    }

    const auto area = contentArea();

    int cols, rows, cellW, imgH, gap, titleH;
    int pageSize;

    if (key == "music") {
        const int rowH = m_metrics.value("music_row_h");
        rows = qMax(1, int(area.second / rowH));
        cols = 1; cellW = imgH = gap = titleH = 0;
        pageSize = rows;
        m_layout[key] = LayoutInfo{1, rows, pageSize};
    } else {
        planGrid(key, area.first, area.second, cols, rows, cellW, imgH, gap, titleH);
        pageSize = cols * rows;
        m_layout[key] = LayoutInfo{cols, rows, pageSize};
    }

    const int total      = items.size();
    const int totalPages = qMax(1, (total + pageSize - 1) / pageSize);
    int idx = m_sel.value(key);
    int page = qMin(idx / pageSize, totalPages - 1);
    const int start = page * pageSize;

    m_page->setText(QString("第 %1/%2 页").arg(page + 1).arg(totalPages));

    if (key == "music")
        renderMusic(items, start, pageSize, idx);
    else
        renderCards(key, cat.glyph, items, start, pageSize,
                    cols, cellW, imgH, gap, titleH, idx);
}

void MediaApp::renderActionPage()
{
    const Action &act = ACTIONS[m_menuIndex - N_CAT];
    clearContent();
    m_title->setText(act.name);
    m_page->setText("");

    auto *box = new QFrame();
    box->setStyleSheet(QString("QFrame { background-color: %1; border: none; }").arg(C::PANEL));
    auto *lay = new QVBoxLayout(box);
    lay->setContentsMargins(px(120), px(40), px(120), px(40));
    lay->setSpacing(px(10));

    auto *big = new QLabel(act.name, box);
    big->setAlignment(Qt::AlignCenter);
    big->setFont(appFont(52, true));
    big->setStyleSheet(QString("color: %1; background: transparent;").arg(C::DANGER));
    big->setFocusPolicy(Qt::NoFocus);
    lay->addWidget(big);

    const QString desc = (act.key == "quit") ? "退出媒体中心并关闭程序"
                                             : "关闭这台电脑";
    auto *d = new QLabel(desc, box);
    d->setAlignment(Qt::AlignCenter);
    d->setFont(appFont(20));
    d->setStyleSheet(QString("color: %1; background: transparent;").arg(C::TEXT));
    d->setFocusPolicy(Qt::NoFocus);
    lay->addWidget(d);

    auto *t = new QLabel(QString("按 Enter 执行（将有 %1 秒倒计时确认）   Esc 取消")
                         .arg(COUNTDOWN_SECONDS), box);
    t->setAlignment(Qt::AlignCenter);
    t->setFont(appFont(14));
    t->setStyleSheet(QString("color: %1; background: transparent;").arg(C::SUB));
    t->setFocusPolicy(Qt::NoFocus);
    lay->addWidget(t);

    auto *wrap = new QWidget();
    auto *wl = new QHBoxLayout(wrap);
    wl->setContentsMargins(0, 0, 0, 0);
    wl->addStretch(1);
    wl->addWidget(box);
    wl->addStretch(1);

    m_contentBox->addStretch(1);
    m_contentBox->addWidget(wrap);
    m_contentBox->addStretch(1);
}

void MediaApp::renderCards(const QString &key, const QString &glyph,
                           const QVector<MediaItem> &items,
                           int start, int pageSize,
                           int cols, int cellW, int imgH, int gap, int titleH,
                           int idx)
{
    auto *host = new QWidget();
    host->setStyleSheet("background: transparent;");
    host->setFocusPolicy(Qt::NoFocus);

    auto *grid = new QGridLayout(host);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(gap);

    const QFont fontN = appFont(15, false);
    const QFont fontB = appFont(15, true);

    for (int slot = 0; slot < pageSize; ++slot) {
        const int pos = start + slot;
        if (pos >= items.size()) break;
        const MediaItem &item = items[pos];
        const bool selected = (m_zone == "right" && pos == idx);
        const int r = slot / cols;
        const int c = slot % cols;

        auto *card = new CardWidget(pos, cellW, imgH, titleH, host);
        const QString posterPath = !item.poster.isEmpty()
            ? item.poster
            : (key == "pic" ? item.path : QString());

        card->setPixmap(m_cache->get(posterPath, cellW - 16, imgH - 10, glyph));
        card->setTitleText(item.disp.isEmpty() ? item.name : item.disp,
                           fontN, fontB, cellW - 20);
        card->setSelected(selected);
        card->onClick = [this](int p){ onCardClicked(p); };

        grid->addWidget(card, r, c, Qt::AlignCenter);
        m_cardWidgets.insert(pos, card);
    }

    m_contentBox->addWidget(host);
    m_contentBox->addStretch(1);
}

void MediaApp::renderMusic(const QVector<MediaItem> &items,
                           int start, int pageSize, int idx)
{
    auto *host = new QWidget();
    host->setStyleSheet("background: transparent;");
    host->setFocusPolicy(Qt::NoFocus);
    auto *vbox = new QVBoxLayout(host);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(px(4));

    const int rowH = m_metrics.value("music_row_h");
    const QFont fontN = appFont(17, false);
    const QFont fontB = appFont(17, true);
    const QFont fontS = appFont(12);

    for (int slot = 0; slot < pageSize; ++slot) {
        const int pos = start + slot;
        if (pos >= items.size()) break;
        const MediaItem &item = items[pos];
        const bool selected = (m_zone == "right" && pos == idx);

        auto *row = new MusicRow(pos, pos + 1, rowH, host);
        const QString folder = QFileInfo(item.path).dir().dirName();
        row->setContent(item.disp.isEmpty() ? item.name : item.disp,
                        folder, fontN, fontB, fontS, selected);
        row->onClick = [this](int p){ onCardClicked(p); };

        vbox->addWidget(row);
        m_musicRows.insert(pos, row);
    }

    vbox->addStretch(1);
    m_contentBox->addWidget(host, 1);
}

// ---------------- 选中态原地刷新 ----------------
void MediaApp::updateSelectionDisplay(const QString &key, int oldPos, int newPos)
{
    const bool on = (m_zone == "right");

    const QFont fontN = appFont(key == "music" ? 17 : 15, false);
    const QFont fontB = appFont(key == "music" ? 17 : 15, true);
    const QFont fontS = appFont(12);

    QList<QPair<int,bool>> pairs;
    pairs << qMakePair(oldPos, false);
    if (newPos != oldPos) pairs << qMakePair(newPos, on);

    const auto &items = m_items.value(key);

    for (auto &p : pairs) {
        const int pos = p.first;
        const bool selected = p.second;

        if (key == "music") {
            auto *w = m_musicRows.value(pos, nullptr);
            if (!w || pos >= items.size()) continue;
            const MediaItem &item = items[pos];
            const QString folder = QFileInfo(item.path).dir().dirName();
            w->setContent(item.disp.isEmpty() ? item.name : item.disp,
                          folder, fontN, fontB, fontS, selected);
        } else {
            auto *w = m_cardWidgets.value(pos, nullptr);
            if (!w || pos >= items.size()) continue;
            w->setSelected(selected);
            w->setTitleText(items[pos].disp.isEmpty() ? items[pos].name : items[pos].disp,
                            fontN, fontB, w->width() - 20);
        }
    }
}

void MediaApp::refreshRightSelection(const QString &key)
{
    const int idx = m_sel.value(key);
    const bool on = (m_zone == "right");
    const auto &items = m_items.value(key);

    if (idx >= items.size()) return;

    if (key == "music") {
        auto *w = m_musicRows.value(idx, nullptr);
        if (!w) return;
        const MediaItem &item = items[idx];
        const QString folder = QFileInfo(item.path).dir().dirName();
        w->setContent(item.disp.isEmpty() ? item.name : item.disp,
                      folder,
                      appFont(17, false), appFont(17, true), appFont(12), on);
    } else {
        auto *w = m_cardWidgets.value(idx, nullptr);
        if (w) w->setSelected(on);
    }
}

void MediaApp::moveSelectionVisual(int delta)
{
    const QString key = CATEGORIES[m_menuIndex].key;
    const int total = m_items.value(key).size();
    if (!total) return;

    const int oldPos = m_sel.value(key);
    const int newPos = qBound(0, oldPos + delta, total - 1);
    if (newPos == oldPos) return;
    m_sel[key] = newPos;

    const int pageSize = qMax(1, m_layout.value(key).pageSize);
    if (oldPos / pageSize != newPos / pageSize) {
        renderContent();
    } else if (m_zone == "right") {
        updateSelectionDisplay(key, oldPos, newPos);
    }
}

int MediaApp::stepVertical(const QString &key) const
{
    return m_layout.value(key).cols;
}

// ---------------- 键盘导航 ----------------
void MediaApp::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Up:      onUp();    break;
    case Qt::Key_Down:    onDown();  break;
    case Qt::Key_Left:    onLeft();  break;
    case Qt::Key_Right:   onRight(); break;
    case Qt::Key_Return:
    case Qt::Key_Enter:   onEnter(); break;
    case Qt::Key_Escape:  onEscape(); break;
    case Qt::Key_F5:      rescanLibrary(); break;
    default:
        QWidget::keyPressEvent(e);
        return;
    }
    e->accept();
}

void MediaApp::onUp()
{
    if (m_zone == "left") {
        m_menuIndex = (m_menuIndex - 1 + N_MENU) % N_MENU;
        refreshAll();
    } else {
        const QString key = CATEGORIES[m_menuIndex].key;
        moveSelectionVisual(-stepVertical(key));
    }
}

void MediaApp::onDown()
{
    if (m_zone == "left") {
        m_menuIndex = (m_menuIndex + 1) % N_MENU;
        refreshAll();
    } else {
        const QString key = CATEGORIES[m_menuIndex].key;
        moveSelectionVisual(stepVertical(key));
    }
}

void MediaApp::onLeft()
{
    if (m_zone != "right") return;
    const QString key = CATEGORIES[m_menuIndex].key;
    const int cols = m_layout.value(key).cols;
    if (cols <= 1 || m_sel.value(key) % cols == 0) {
        m_zone = "left";
        renderSidebar();
        refreshRightSelection(key);
    } else {
        moveSelectionVisual(-1);
    }
}

void MediaApp::onRight()
{
    if (m_zone == "left") {
        if (m_menuIndex < N_CAT) {
            const QString key = CATEGORIES[m_menuIndex].key;
            if (!m_items.value(key).isEmpty()) {
                m_zone = "right";
                renderSidebar();
                refreshRightSelection(key);
            }
        }
    } else {
        moveSelectionVisual(1);
    }
}

void MediaApp::onEnter()
{
    if (m_zone == "left") {
        if (m_menuIndex >= N_CAT) {
            requestAction(ACTIONS[m_menuIndex - N_CAT].key);
        } else {
            const QString key = CATEGORIES[m_menuIndex].key;
            if (!m_items.value(key).isEmpty()) {
                m_zone = "right";
                renderSidebar();
                refreshRightSelection(key);
            }
        }
        return;
    }
    const QString key = CATEGORIES[m_menuIndex].key;
    const auto &items = m_items.value(key);
    if (items.isEmpty()) return;
    activate(items.value(m_sel.value(key)));
}

void MediaApp::onEscape()
{
    if (m_zone == "right") {
        const QString key = CATEGORIES[m_menuIndex].key;
        m_zone = "left";
        renderSidebar();
        refreshRightSelection(key);
    }
}

void MediaApp::refreshAll()
{
    renderSidebar();
    renderContent();
}

// ---------------- 鼠标 ----------------
void MediaApp::onMenuClicked(int idx)
{
    m_menuIndex = idx;
    m_zone = "left";
    refreshAll();
    if (idx >= N_CAT) requestAction(ACTIONS[idx - N_CAT].key);
}

void MediaApp::onCardClicked(int pos)
{
    const QString key = CATEGORIES[m_menuIndex].key;
    const auto &items = m_items.value(key);
    if (pos < 0 || pos >= items.size()) return;

    const int oldPos = m_sel.value(key);
    m_sel[key] = pos;
    m_zone = "right";
    if (oldPos != pos) updateSelectionDisplay(key, oldPos, pos);
    else               refreshRightSelection(key);
    renderSidebar();
    activate(items[pos]);
}

// ---------------- 退出 / 关机 ----------------
void MediaApp::requestAction(const QString &actionKey)
{
    if (m_confirm || m_exiting) return;
    m_confirm = new CountdownDialog(this, actionKey);
    m_confirm->open();
}

void MediaApp::executeAction(const QString &actionKey)
{
    if (m_exiting) return;
    m_exiting = true;

    if (m_player) { m_player->close(); m_player = nullptr; }
    if (m_viewer) { m_viewer->close(); m_viewer = nullptr; }

    if (actionKey == "shutdown") {
        bool ok = true;
#ifdef Q_OS_WIN
        ok = QProcess::startDetached("shutdown", {"/s", "/t", "0"});
#else
        ok = QProcess::startDetached("systemctl", {"poweroff"});
#endif
        if (!ok) {
            m_exiting = false;
            m_status->setText("关机命令执行失败");
            return;
        }
    }
    qApp->quit();
}

// ---------------- 激活 ----------------
void MediaApp::activate(const MediaItem &item)
{
    if (item.type == "tv")        openEpisodeDialog(item);
    else if (item.type == "pic")  openPictureViewer(item);
    else                          playMedia(item.path,
                                            item.disp.isEmpty() ? item.name : item.disp);
}

void MediaApp::playMedia(const QString &path, const QString &title)
{
    if (m_player) { m_player->close(); m_player = nullptr; }

    // parent 传 nullptr 让 PlayerWindow 成为 toplevel 窗口。
    // Linux 上 libvlc_media_player_set_xwindow 只接受 toplevel XID，
    // subwindow XID 会被 VLC 忽略、回退到自己开窗口。
    m_player = new PlayerWindow(path, title, nullptr);
    m_player->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(m_player, &QObject::destroyed, this, [this](){
        m_player = nullptr;
        showFullScreen();
        raise();
        activateWindow();
        setFocus();
    });
    m_player->setWindowState(Qt::WindowFullScreen);
    m_player->show();
    m_player->raise();
    m_player->activateWindow();
    m_player->setFocus();
}

void MediaApp::openEpisodeDialog(const MediaItem &item)
{
    const QStringList eps = listTvEpisodes(item.path);
    if (eps.isEmpty()) {
        m_status->setText(QString("《%1》目录下没有可播放的文件").arg(item.name));
        return;
    }
    EpisodeDialog dlg(this, item, eps);
    if (dlg.exec() == QDialog::Accepted && !dlg.resultPath().isEmpty())
        playMedia(dlg.resultPath(), dlg.resultTitle());
    setFocus();
}

void MediaApp::openPictureViewer(const MediaItem &item)
{
    const auto &pics = m_items.value("pic");
    int idx = 0;
    for (int i = 0; i < pics.size(); ++i)
        if (pics[i].path == item.path) { idx = i; break; }

    if (m_viewer) { m_viewer->close(); m_viewer = nullptr; }

    // 同理传 nullptr 让 PictureViewer 成为 toplevel
    m_viewer = new PictureViewer(pics, idx, nullptr);
    m_viewer->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(m_viewer, &QObject::destroyed, this, [this](){
        m_viewer = nullptr;
        showFullScreen();
        raise();
        activateWindow();
        setFocus();
    });
    m_viewer->setWindowState(Qt::WindowFullScreen);
    m_viewer->show();
    m_viewer->raise();
    m_viewer->activateWindow();
    m_viewer->setFocus();
}

void MediaApp::openExternal(const QString &path)
{
    bool ok = false;
#ifdef Q_OS_WIN
    ok = QProcess::startDetached("cmd", {"/c", "start", "", path});
#elif defined(Q_OS_MAC)
    ok = QProcess::startDetached("open", {path});
#else
    ok = QProcess::startDetached("xdg-open", {path});
#endif
    if (!ok) m_status->setText(QString("无法播放 %1").arg(path));
}

void MediaApp::focusMainWindow()
{
    showFullScreen();
    raise();
    activateWindow();
    setFocus();
}