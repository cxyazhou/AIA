#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVector>
#include <QtCore/QSet>
#include <QtCore/QHash>
#include <QtCore/QList>

#include <QtGui/QPixmap>

struct MediaItem {
    QString type;    // tv / movie / pic / music
    QString name;
    QString disp;
    QString path;
    QString poster;
};

struct Category { QString key, name, fileName, glyph; };
struct Action   { QString key, name, glyph; };

namespace C {
    extern const QString BG, PANEL, CARD, CARD_SEL, TEXT, SUB, ACCENT,
                         SEL_BG, DANGER, DANGER_BG;
}

extern const QString FONT_FAMILY;
extern const int COUNTDOWN_SECONDS;

extern const QVector<Category> CATEGORIES;
extern const QVector<Action>   ACTIONS;
extern const QSet<QString>     VIDEO_EXTS;
extern const QSet<QString>     AUDIO_EXTS;
extern const QSet<QString>     IMAGE_EXTS;
extern const QStringList       POSTER_EXTS;
extern const int N_CAT;
extern const int N_MENU;

QString  baseDir();
QStringList readListFile(const QString &path);
bool     naturalLess(const QString &a, const QString &b);

QVector<MediaItem> scanTv(const QStringList &paths);
QVector<MediaItem> scanFiles(const QStringList &paths,
                             const QSet<QString> &exts,
                             const QString &itemType);
QString findTvPoster(const QString &dirPath);
QString findMoviePoster(const QString &filePath);
QStringList listTvEpisodes(const QString &dirPath);

QPixmap loadPixmapFile(const QString &path);
QPixmap coverResizePixmap(const QPixmap &pix, int w, int h);
QPixmap makePlaceholder(int w, int h, const QString &glyph);

class ThumbCache {
public:
    explicit ThumbCache(int limit = 400) : m_limit(limit) {}
    QPixmap get(const QString &path, int w, int h,
                const QString &glyph = QString());
    void clear();
private:
    int m_limit;
    QHash<QString, QPixmap> m_data;
    QList<QString> m_order;
};