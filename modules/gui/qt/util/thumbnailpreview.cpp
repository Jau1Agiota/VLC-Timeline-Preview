/* Native timeline previews. GPL-2.0-or-later; see COPYING. */
#include "thumbnailpreview.hpp"
#include <QApplication>
#include <QWidget>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QEvent>
#include <QFile>
#include <QCryptographicHash>
#ifdef Q_OS_WIN
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <windows.h>
#endif

namespace TimelinePreview {
// GUI-thread weak ownership; no queue across hidden/fullscreen timelines.
static QPointer<Controller> activePreview;
static QPointer<Controller> extracting;
static QSharedPointer<Cache> sharedCache()
{
    // All controllers live on the GUI thread. One 8 MiB/32-entry budget across
    // main-window and fullscreen timelines; release it with the last widget.
    static QWeakPointer<Cache> weak;
    QSharedPointer<Cache> cache = weak.toStrongRef();
    if(!cache) { cache.reset(new Cache); weak = cache; }
    return cache;
}
qint64 bucket(qint64 ms) { return ms < 0 || ms > 86400000 ? -1 : ms / 250 * 250; }

QRect popupRect(const QSize &size, const QPoint &anchor, const QRect &bounds)
{
    if(size.isEmpty() || !bounds.isValid() || !bounds.contains(anchor) || size.width() > bounds.width() ||
       size.height() + 6 > anchor.y() - bounds.top()) return QRect();
    return QRect(QPoint(qBound(bounds.left(), anchor.x() - size.width()/2,
                              bounds.right() - size.width() + 1),
                        anchor.y() - size.height() - 6), size);
}
static qint64 imageBytes(const QImage &image) { return qint64(image.bytesPerLine()) * image.height(); }
Cache::Cache(int entries, qint64 bytes) : maxEntries(entries), maxBytes(bytes) {}
void Cache::clear() { images.clear(); lru.clear(); used = 0; }
QImage Cache::get(const QString &key)
{
    if(!images.contains(key)) return QImage();
    lru.removeAll(key); lru.append(key); return images.value(key);
}
void Cache::put(const QString &key, const QImage &image)
{
    const qint64 cost = (qint64(image.bytesPerLine()) * image.height());
    if(image.isNull() || image.width() > 320 || image.height() > 180 ||
       cost > maxBytes || maxEntries <= 0 || key.size() > 64) return;
    if(images.contains(key)) { used -= imageBytes(images.take(key)); lru.removeAll(key); }
    while(!lru.isEmpty() && (images.size() >= maxEntries || used + cost > maxBytes)) {
        used -= imageBytes(images.take(lru.takeFirst()));
    }
    images.insert(key, image); lru.append(key); used += cost;
}

static bool localPath(const QString &path)
{
    if(!QDir::isAbsolutePath(path)) return false;
#ifdef Q_OS_WIN
    // Do not stat UNC/mapped-network/removable paths on the GUI thread.
    if(path.size() < 3 || path.at(1) != QLatin1Char(':')) return false;
    const wchar_t root[] = { wchar_t(path.at(0).unicode()), L':', L'\\', 0 };
    if(GetDriveTypeW(root) != DRIVE_FIXED) return false;
#endif
    return true;
}
QString Controller::identity(const QString &path)
{
    if(!localPath(path)) return QString();
    QFileInfo info(path);
    if((info.suffix().compare(QStringLiteral("mp4"), Qt::CaseInsensitive) &&
        info.suffix().compare(QStringLiteral("mkv"), Qt::CaseInsensitive)) ||
       !info.isFile() || !info.isReadable()) return QString();
    const QString canonical = info.canonicalFilePath();
    if(canonical.isEmpty() || !localPath(canonical)) return QString();
    return canonical + QChar(0) + QString::number(info.size()) + QChar(0) +
           QString::number(info.lastModified().toMSecsSinceEpoch());
}
Controller::Controller(QWidget *widget, Current check, Present update, Log logger,
                       const QString &program, int delay, int timeout)
    : QObject(widget), host(widget), current(check), present(update), log(logger),
      helper(program.isEmpty() ? QCoreApplication::applicationDirPath() +
             QStringLiteral("/thumbnail-preview/thumbnail-helper.exe") : program), cache(sharedCache())
{
    debounce.setSingleShot(true); debounce.setInterval(delay);
    deadline.setSingleShot(true); deadline.setInterval(timeout);
    health.setInterval(100);
    connect(&debounce, &QTimer::timeout, this, [this] { launch(); });
    connect(&deadline, &QTimer::timeout, this, [this] {
        if(running.generation != generation) { stopProcess(); return; }
        failed(QStringLiteral("helper timeout")); cancel();
    });
    connect(&health, &QTimer::timeout, this, [this] {
        if(!current(wanted.token) || identity(wanted.path) != wanted.identity) cancel(true);
    });
    connect(&process, &QProcess::readyReadStandardOutput, this, [this] { drain(); });
    connect(&process, &QProcess::readyReadStandardError, this, [this] { drain(); });
    connect(&process, static_cast<void(QProcess::*)(int,QProcess::ExitStatus)>(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus status) { finished(code,status); });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if(error == QProcess::FailedToStart) {
            deadline.stop();
            if(extracting == this) extracting.clear();
            if(running.generation == generation) { pending = false; failed(QStringLiteral("helper start failed")); }
            if(!output.isEmpty()) QFile::remove(output);
            if(pending && !debounce.isActive()) QTimer::singleShot(0,this,[this] { launch(); });
        }
    });
#ifdef Q_OS_WIN
    HANDLE handle = CreateJobObjectW(nullptr, nullptr);
    if(handle) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                                 JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        limits.ProcessMemoryLimit = SIZE_T(512) * 1024 * 1024;
        if(SetInformationJobObject(handle, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) job = handle;
        else CloseHandle(handle);
    }
#endif
    connect(&process, &QProcess::started, this, [this] {
#ifdef Q_OS_WIN
        HANDLE child = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE | PROCESS_SET_INFORMATION,
                                   FALSE, DWORD(process.processId()));
        const bool assigned = job && child && AssignProcessToJobObject(HANDLE(job), child);
        if(assigned) SetPriorityClass(child, BELOW_NORMAL_PRIORITY_CLASS);
        if(child) CloseHandle(child);
        if(!assigned) { failed(QStringLiteral("cannot contain helper in job")); cancel(); return; }
#endif
        if(running.generation != generation) stopProcess();
    });
    qApp->installEventFilter(this);
}
Controller::~Controller()
{
    closing = true;
    disconnect(&process, nullptr, this, nullptr);
    cancel(true);
    deadline.stop();
    if(extracting == this) extracting.clear();
#ifdef Q_OS_WIN
    if(job) { CloseHandle(HANDLE(job)); job = nullptr; }
#endif
    // Only teardown waits, never extraction/hover. kill targets this private child.
    if(process.state() != QProcess::NotRunning) {
        process.kill(); process.waitForFinished(1000);
    }
}
void Controller::diagnose(const QString &stage, const QString &detail, int intervalMs) const
{
    if(trace.isEnabled()) trace.record(stage, QStringLiteral("component=controller controller=%1 stage=%2 %3")
        .arg(diagnosticId(this)).arg(stage).arg(detail), intervalMs);
}
void Controller::hide() { visible = false; present(QImage(), -1); }
void Controller::stopProcess()
{
#ifdef Q_OS_WIN
    if(job) TerminateJobObject(HANDLE(job), 1);
#endif
    if(process.state() != QProcess::NotRunning) process.kill();
}
void Controller::cancel(bool clearCache)
{
    if(trace.isEnabled()) diagnose(QStringLiteral("cancel"), QStringLiteral("clear_cache=%1 pending=%2 visible=%3 process_state=%4")
        .arg(clearCache).arg(pending).arg(visible).arg(int(process.state())), 0);
    ++generation; pending = false; wanted = Request();
    debounce.stop(); health.stop(); hide(); stopProcess();
    // Keep the deadline during Starting: started() also rejects canceled work.
    if(process.state() == QProcess::NotRunning) {
        deadline.stop(); if(!output.isEmpty()) QFile::remove(output);
        if(extracting == this) extracting.clear();
    }
    if(activePreview == this) activePreview.clear();
    if(clearCache) { cache->clear(); sourceKey.clear(); failedKey.clear(); failedAt.invalidate(); }
}
void Controller::failed(const QString &reason)
{
    diagnose(QStringLiteral("failure"), reason, 0);
    failedKey = running.key.isEmpty() ? wanted.key : running.key;
    failedAt.restart(); hide(); if(log) log(reason);
}
bool Controller::eventFilter(QObject *object, QEvent *event)
{
    // Reuse the existing global observer even before the first eligible hover.
    // Do not install SeekSlider globally: its local tooltip filter has actions.
    if(trace.isEnabled() &&
       ((object == qApp && (event->type() == QEvent::ApplicationActivate ||
                           event->type() == QEvent::ApplicationDeactivate ||
                           event->type() == QEvent::ApplicationStateChange)) ||
        (object == host->window() && (event->type() == QEvent::WindowActivate ||
                                     event->type() == QEvent::WindowDeactivate ||
                                     event->type() == QEvent::WindowStateChange ||
                                     event->type() == QEvent::Show || event->type() == QEvent::Hide))))
        diagnose(QStringLiteral("activation"), QStringLiteral(
            "event=%1 object=%2 host_visible=%3 under_mouse=%4 app_state=%5 owner_rect=%6")
            .arg(diagnosticEvent(event->type())).arg(diagnosticId(object)).arg(host->isVisible())
            .arg(host->underMouse()).arg(int(qApp->applicationState()))
            .arg(diagnosticRect(QRect(host->window()->mapToGlobal(QPoint(0,0)),host->window()->size()))), 0);
    if((object == qApp && event->type() == QEvent::ApplicationDeactivate) ||
       (object == ownerWindow && (event->type() == QEvent::Hide || event->type() == QEvent::Close ||
         event->type() == QEvent::WindowDeactivate || event->type() == QEvent::Move || event->type() == QEvent::Resize)))
    {
        if(trace.isEnabled()) diagnose(QStringLiteral("window-event"), QStringLiteral("event=%1 object=%2 app_state=%3")
            .arg(diagnosticEvent(event->type())).arg(diagnosticId(object)).arg(int(qApp->applicationState())), 0);
        cancel();
    }
    return QObject::eventFilter(object,event);
}
void Controller::hover(const QString &path, const QString &token, qint64 ms)
{
    if(closing) return;
    const qint64 time = bucket(ms);
    const QString file = identity(path);
    if(trace.isEnabled()) diagnose(QStringLiteral("hover"), QStringLiteral("requested_ms=%1 bucket_ms=%2 closing=%3 token_empty=%4 local_identity_ok=%5")
        .arg(ms).arg(time).arg(closing).arg(token.isEmpty()).arg(!file.isEmpty()));
    if(time < 0 || token.isEmpty() || file.isEmpty() || !current(token)) { cancel(); return; }
    const QString source = token + QChar(0) + file;
    if(activePreview && activePreview != this) activePreview->cancel();
    activePreview = this;
    if(sourceKey != source) { cancel(); sourceKey = source; }
    activePreview = this;
    if(ownerWindow != host->window()) {
        if(ownerWindow) ownerWindow->removeEventFilter(this);
        ownerWindow = host->window(); ownerWindow->installEventFilter(this);
    }
    const QString key = QString::fromLatin1(QCryptographicHash::hash(
        (source + QChar(0) + QString::number(time)).toUtf8(), QCryptographicHash::Sha256).toHex());
    health.start();
    if(wanted.key == key) {
        // Same bucket does not restart a debounce/running extraction. A later
        // mouse event may retry a failure after backoff; no autonomous retry loop.
        if(!visible && !pending && process.state() == QProcess::NotRunning &&
           key == failedKey && failedAt.isValid() && failedAt.elapsed() >= 2000) {
            pending = true; debounce.start();
        }
        return;
    }
    ++generation; wanted.path = path; wanted.token = token; wanted.identity = file;
    wanted.time = time; wanted.key = key; wanted.generation = generation;
    pending = false; debounce.stop(); hide(); stopProcess();
    QImage image = cache->get(key);
    if(!image.isNull()) {
        diagnose(QStringLiteral("cache-hit"), QStringLiteral("bucket_ms=%1").arg(time));
        visible = true; present(image,time); return;
    }
    if(key == failedKey && failedAt.isValid() && failedAt.elapsed() < 2000) return;
    pending = true; debounce.start();
}
void Controller::launch()
{
    if(trace.isEnabled()) diagnose(QStringLiteral("launch"), QStringLiteral("closing=%1 pending=%2 debounce=%3 process_state=%4 other_controller=%5 bucket_ms=%6")
        .arg(closing).arg(pending).arg(debounce.isActive()).arg(int(process.state()))
        .arg(diagnosticId(extracting.data())).arg(wanted.time));
    if(closing || !pending || debounce.isActive()) return;
    if(process.state() != QProcess::NotRunning) return; // finish schedules latest work
    if(extracting && extracting != this && extracting->process.state() != QProcess::NotRunning) {
        extracting->cancel();
        QTimer::singleShot(20,this,[this] { launch(); }); // wait for prior child to be reaped
        return;
    }
    if(!current(wanted.token) || identity(wanted.path) != wanted.identity) { cancel(true); return; }
    pending = false; running = wanted; stdoutData.clear();
    if(!localPath(helper) || !QFileInfo(helper).isExecutable()) { failed(QStringLiteral("helper unavailable")); return; }
    if(!localPath(QDir::tempPath())) { failed(QStringLiteral("temporary path is not a fixed local drive")); return; }
    if(!directory) directory.reset(new QTemporaryDir(QDir::tempPath() + QStringLiteral("/vlc-preview-XXXXXX")));
    if(!directory->isValid()) { failed(QStringLiteral("temporary directory unavailable")); return; }
    output = directory->path() + QStringLiteral("/frame.bmp");
    if(!output.isEmpty()) QFile::remove(output);
    if(QFileInfo::exists(output)) { failed(QStringLiteral("temporary frame still locked")); return; }
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("VLC_PLUGIN_PATH"));
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(QFileInfo(helper).absolutePath());
    deadline.start(); ++launches; extracting = this;
    diagnose(QStringLiteral("spawn"), QStringLiteral("bucket_ms=%1 generation=%2").arg(wanted.time).arg(generation), 0);
    process.start(helper, QStringList() << wanted.path << QString::number(wanted.time) << output << QStringLiteral("5000"));
}
void Controller::drain()
{
    process.setReadChannel(QProcess::StandardOutput);
    while(process.bytesAvailable()) {
        const QByteArray chunk = process.read(4096);
        if(stdoutData.size() < 4096) stdoutData.append(chunk.left(4096 - stdoutData.size()));
    }
    process.setReadChannel(QProcess::StandardError);
    while(process.bytesAvailable()) process.read(4096); // diagnostics never accumulate
}
void Controller::finished(int code, QProcess::ExitStatus status)
{
    deadline.stop(); drain();
    if(extracting == this) extracting.clear();
#ifdef Q_OS_WIN
    if(job) TerminateJobObject(HANDLE(job), 1); // no descendants survive a completed request
#endif
    const bool latest = running.generation == generation && running.key == wanted.key &&
                        current(running.token) && identity(running.path) == running.identity;
    if(trace.isEnabled()) diagnose(QStringLiteral("finished"), QStringLiteral("exit=%1 exit_status=%2 latest=%3 bucket_ms=%4 running_generation=%5 generation=%6 output_bytes=%7")
        .arg(code).arg(int(status)).arg(latest).arg(running.time).arg(running.generation).arg(generation).arg(stdoutData.size()), 0);
    if(latest) {
        const QJsonObject record = QJsonDocument::fromJson(stdoutData).object();
        const QString result = record.value(QStringLiteral("status")).toString();
        const QFileInfo info(output);
        const bool boundedFile = info.size() > 54 && info.size() <= 320 * 180 * 4 + 54;
        QImageReader reader(boundedFile ? output : QString(), "bmp");
        const QSize size = boundedFile ? reader.size() : QSize();
        if(status == QProcess::NormalExit && code == 0 &&
           (result == QLatin1String("candidate_captured") || result == QLatin1String("fallback_candidate_captured")) &&
           record.value(QStringLiteral("requested_ms")).toDouble(-1) == running.time &&
           boundedFile &&
           size.width() > 0 && size.width() <= 320 && size.height() > 0 && size.height() <= 180 &&
           record.value(QStringLiteral("width")).toInt() == size.width() &&
           record.value(QStringLiteral("height")).toInt() == size.height()) {
            QImage image = reader.read().convertToFormat(QImage::Format_ARGB32);
            if(!image.isNull()) {
                cache->put(running.key,image); visible = true; present(image,running.time);
            } else failed(QStringLiteral("invalid helper image"));
        } else failed(QStringLiteral("helper did not return a bounded candidate"));
    }
    if(!output.isEmpty()) QFile::remove(output);
    if(pending && !debounce.isActive()) QTimer::singleShot(0,this,[this] { launch(); });
}
}
