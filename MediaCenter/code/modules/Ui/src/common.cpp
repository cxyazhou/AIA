#include "common.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtCore/QSet>

#include <QtGui/QImageReader>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QFont>
#include <QtGui/QColor>

#include <algorithm>

namespace C {
    const QString BG        = "#000000";
    const QString PANEL     = "#0d0d0d";
    const QString CARD      = "#1b1b1b";
    const QString CARD_SEL  = "#3498db";
    const QString TEXT      = "#f2f2f2";
    const QString SUB       = "#8a8a8a";
    const QString ACCENT    = "#3498db";
    const QString SEL_BG    = "#14304a";
    const QString DANGER    = "#e74c3c";
    const QString DANGER_BG = "#3a1715";
}

const QString FONT_FAMILY   = QStringLiteral("Microsoft YaHei UI");
const int     COUNTDOWN_SECONDS = 5;

const QVector<Category> CATEGORIES = {
    {"tv",    "电视剧", "MediaConfig/tv.cfg",    "剧"},
    {"movie", "电影",   "MediaConfig/movie.cfg", "影"},
    {"pic",   "图片",   "MediaConfig/pic.cfg",   "图"},
    {"music", "音乐",   "MediaConfig/music.cfg", "♪"},
};
const QVector<Action> ACTIONS = {
    {"quit",     "退出", "出"},
    {"shutdown", "关机", "关"},
};
const int N_CAT  = CATEGORIES.size();
const int N_MENU = N_CAT + ACTIONS.size();

const QSet<QString> VIDEO_EXTS = {
    ".mp4",".mkv",".avi",".rmvb",".rm",".ts",".m2ts",".mov",
    ".wmv",".flv",".f4v",".m4v",".mpg",".mpeg",".3gp",".webm",
    ".iso",".vob"
};
const QSet<QString> AUDIO_EXTS = {
    ".mp3",".flac",".wav",".ape",".m4a",".aac",".ogg",
    ".wma",".opus",".aiff",".cue"
};
const QSet<QString> IMAGE_EXTS = {
    ".jpg",".jpeg",".png",".bmp",".gif",".webp",".tif",".tiff"
};
const QStringList POSTER_EXTS = {".jpg",".jpeg",".png",".webp",".avif"};

// ---------------- 工具 ----------------

QString baseDir()
{
    return QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
}

QStringList readListFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QByteArray raw = f.readAll();
    QString text = QString::fromUtf8(raw);
    if (text.contains(QChar(0xFFFD)))
        text = QString::fromLocal8Bit(raw);

    QStringList out;
    const QStringList lines = text.split(QRegularExpression("[\r\n]"));
    for (const QString &line : lines) {
        QString p = line.trimmed();
        if (p.isEmpty() || p.startsWith('#')) continue;
        // 展开 %VAR% 环境变量（简版）
        static QRegularExpression reEnv("%(\\w+)%");
        auto it = reEnv.globalMatch(p);
        QString tmp = p;
        while (it.hasNext()) {
            auto m = it.next();
            QString val = qEnvironmentVariable(m.captured(1).toUtf8().constData());
            if (!val.isEmpty()) tmp.replace(m.captured(0), val);
        }
        out << QDir::cleanPath(QFileInfo(tmp).absoluteFilePath());
    }
    return out;
}

static QStringList natSplit(const QString &s)
{
    QStringList out;
    QString cur;
    bool lastDigit = false;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        const bool d = c.isDigit();
        if (i > 0 && d != lastDigit) { out << cur; cur.clear(); }
        cur += c;
        lastDigit = d;
    }
    if (!cur.isEmpty()) out << cur;
    return out;
}

bool naturalLess(const QString &a, const QString &b)
{
    const QStringList pa = natSplit(a.toLower());
    const QStringList pb = natSplit(b.toLower());
    const int n = qMin(pa.size(), pb.size());
    for (int i = 0; i < n; ++i) {
        bool oka = false, okb = false;
        const qlonglong na = pa[i].toLongLong(&oka);
        const qlonglong nb = pb[i].toLongLong(&okb);
        if (oka && okb) {
            if (na != nb) return na < nb;
        } else {
            const int c = QString::compare(pa[i], pb[i]);
            if (c != 0) return c < 0;
        }
    }
    return pa.size() < pb.size();
}

QVector<MediaItem> scanTv(const QStringList &paths)
{
    QVector<MediaItem> items;
    for (const QString &root : paths) {
        QDir dir(root);
        if (!dir.exists()) { qWarning("跳过不存在路径: %s", qPrintable(root)); continue; }
        const auto list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &fi : list) {
            MediaItem it;
            it.type = "tv";
            it.name = fi.fileName();
            it.disp = it.name;
            it.path = fi.absoluteFilePath();
            it.poster = findTvPoster(it.path);
            items << it;
        }
    }
    std::sort(items.begin(), items.end(),
              [](const MediaItem &a, const MediaItem &b) {
                  return naturalLess(a.name, b.name);
              });
    return items;
}

QVector<MediaItem> scanFiles(const QStringList &paths,
                             const QSet<QString> &exts,
                             const QString &itemType)
{
    QVector<MediaItem> items;
    for (const QString &root : paths) {
        QDir dir(root);
        if (!dir.exists()) { qWarning("跳过不存在路径: %s", qPrintable(root)); continue; }
        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo fi = it.fileInfo();
            const QString ext = "." + fi.suffix().toLower();
            if (!exts.contains(ext)) continue;
            MediaItem m;
            m.type = itemType;
            m.name = fi.fileName();
            m.disp = fi.completeBaseName();
            m.path = fi.absoluteFilePath();
            if (itemType == "movie") m.poster = findMoviePoster(m.path);
            items << m;
        }
    }
    std::sort(items.begin(), items.end(),
              [](const MediaItem &a, const MediaItem &b) {
                  return naturalLess(a.path, b.path);
              });
    return items;
}

QString findTvPoster(const QString &dirPath)
{
    QDir d(dirPath);
    const QString base = d.dirName();
    for (const QString &ext : POSTER_EXTS) {
        const QString p = dirPath + "/" + base + ext;
        if (QFile::exists(p)) return p;
    }
    return QString();
}

QString findMoviePoster(const QString &filePath)
{
    QFileInfo fi(filePath);
    const QString stem = fi.absolutePath() + "/" + fi.completeBaseName();
    for (const QString &ext : POSTER_EXTS) {
        const QString p = stem + ext;
        if (QFile::exists(p)) return p;
    }
    return QString();
}

QStringList listTvEpisodes(const QString &dirPath)
{
    QSet<QString> playable = VIDEO_EXTS;
    playable.unite(AUDIO_EXTS);
    QDir d(dirPath);
    QStringList out;
    for (const QFileInfo &fi : d.entryInfoList(QDir::Files)) {
        const QString ext = "." + fi.suffix().toLower();
        if (playable.contains(ext)) out << fi.fileName();
    }
    std::sort(out.begin(), out.end(), naturalLess);
    return out;
}

// ---------------- 图片 ----------------

QPixmap loadPixmapFile(const QString &path)
{
    if (path.isEmpty() || !QFile::exists(path)) return QPixmap();
    QPixmap pix;
    if (pix.load(path)) return pix;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage img = reader.read();
    return img.isNull() ? QPixmap() : QPixmap::fromImage(img);
}

QPixmap coverResizePixmap(const QPixmap &pix, int w, int h)
{
    if (pix.isNull() || w <= 0 || h <= 0) return pix;
    const QPixmap s = pix.scaled(w, h, Qt::KeepAspectRatioByExpanding,
                                 Qt::SmoothTransformation);
    const int x = qMax(0, (s.width()  - w) / 2);
    const int y = qMax(0, (s.height() - h) / 2);
    return s.copy(x, y, w, h);
}

QPixmap makePlaceholder(int w, int h, const QString &glyph)
{
    w = qMax(1, w); h = qMax(1, h);
    QPixmap pix(w, h);
    pix.fill(QColor(26, 26, 30));
    QPainter p(&pix);
    p.setPen(QPen(QColor(70, 70, 78), 1));
    p.drawRect(0, 0, w - 1, h - 1);
    QFont f(FONT_FAMILY);
    f.setPixelSize(qMax(8, int(qMin(w, h) * 0.36)));
    p.setFont(f);
    p.setPen(QColor(110, 110, 120));
    p.drawText(pix.rect(), Qt::AlignCenter, glyph.isEmpty() ? "?" : glyph);
    return pix;
}

QPixmap ThumbCache::get(const QString &path, int w, int h, const QString &glyph)
{
    w = qMax(1, w); h = qMax(1, h);
    const QString key = path + "|" + QString::number(w) + "x" + QString::number(h);
    auto it = m_data.find(key);
    if (it != m_data.end()) return it.value();

    const QPixmap src = loadPixmapFile(path);
    const QPixmap out = src.isNull() ? makePlaceholder(w, h, glyph)
                                     : coverResizePixmap(src, w, h);

    m_data.insert(key, out);
    m_order.append(key);
    if (m_order.size() > m_limit) {
        const int drop = m_order.size() - m_limit;
        for (int i = 0; i < drop; ++i) m_data.remove(m_order[i]);
        m_order = m_order.mid(drop);
    }
    return out;
}

void ThumbCache::clear()
{
    m_data.clear();
    m_order.clear();
}