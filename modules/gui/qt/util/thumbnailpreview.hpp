/* Native timeline preview controller. GPL-2.0-or-later; see COPYING. */
#ifndef VLC_QT_THUMBNAILPREVIEW_HPP
#define VLC_QT_THUMBNAILPREVIEW_HPP

#include <QObject>
#include <QImage>
#include <QMap>
#include <QList>
#include <QProcess>
#include <QTimer>
#include <QTemporaryDir>
#include <QScopedPointer>
#include <QElapsedTimer>
#include <QPointer>
#include <QSharedPointer>
#include <functional>
class QWidget;

namespace TimelinePreview {
// Shared by the GUI and tests; coordinates remain Qt logical pixels.
QRect popupRect(const QSize &size, const QPoint &anchor, const QRect &bounds);
qint64 bucket(qint64 milliseconds);

class Cache {
public:
    explicit Cache(int entries = 32, qint64 bytes = 8 * 1024 * 1024);
    QImage get(const QString &key);
    void put(const QString &key, const QImage &image);
    void clear();
    int count() const { return images.size(); }
    qint64 bytes() const { return used; }
private:
    QMap<QString, QImage> images;
    QList<QString> lru;
    int maxEntries;
    qint64 maxBytes, used = 0;
};

// GUI-thread control only. Decode runs exclusively inside the private helper.
// No Q_OBJECT/moc, blocking start/wait, playback handles, or player controls.
class Controller : public QObject {
public:
    typedef std::function<bool(const QString &)> Current;
    typedef std::function<void(const QImage &, qint64)> Present;
    typedef std::function<void(const QString &)> Log;
    Controller(QWidget *host, Current current, Present present, Log log,
               const QString &helper = QString(), int debounceMs = 150,
               int deadlineMs = 6500);
    ~Controller();
    void hover(const QString &path, const QString &mediaToken, qint64 milliseconds);
    void cancel(bool clearCache = false);
    bool showing() const { return visible; }
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    friend class PreviewTests;
    struct Request {
        QString path, token, identity, key;
        qint64 time = -1;
        quint64 generation = 0;
    } wanted, running;
    QWidget *host;
    QPointer<QWidget> ownerWindow;
    Current current;
    Present present;
    Log log;
    QString helper, output, failedKey, sourceKey;
    QElapsedTimer failedAt;
    QScopedPointer<QTemporaryDir> directory;
    QProcess process;
    QTimer debounce, deadline, health;
    QSharedPointer<Cache> cache;
    QByteArray stdoutData;
    quint64 generation = 0;
    bool pending = false, visible = false, closing = false;
    void *job = nullptr;
    int launches = 0;
    void launch();
    void finished(int, QProcess::ExitStatus);
    void failed(const QString &);
    void hide();
    void drain();
    void stopProcess();
    static QString identity(const QString &);
};
}
#endif
