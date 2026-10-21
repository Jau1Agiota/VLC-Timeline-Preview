/* Debug-only timeline observations. GPL-2.0-or-later; see COPYING. */
#ifndef VLC_QT_PREVIEWDIAGNOSTICS_HPP
#define VLC_QT_PREVIEWDIAGNOSTICS_HPP
#include <QElapsedTimer>
#include <QEvent>
#include <QMap>
#include <QRect>
#include <QString>
#include <functional>

namespace TimelinePreview {
inline QString diagnosticId(const void *object)
{ return QString::number(reinterpret_cast<quintptr>(object), 16); }
inline QString diagnosticRect(const QRect &r)
{ return QStringLiteral("%1,%2,%3,%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()); }
inline QString diagnosticEvent(QEvent::Type type)
{
    switch(type) {
    case QEvent::Show: return QStringLiteral("show");
    case QEvent::Hide: return QStringLiteral("hide");
    case QEvent::Close: return QStringLiteral("close");
    case QEvent::Move: return QStringLiteral("move");
    case QEvent::Resize: return QStringLiteral("resize");
    case QEvent::WindowActivate: return QStringLiteral("activate");
    case QEvent::WindowDeactivate: return QStringLiteral("deactivate");
    case QEvent::WindowStateChange: return QStringLiteral("window-state");
    case QEvent::ApplicationActivate: return QStringLiteral("app-activate");
    case QEvent::ApplicationDeactivate: return QStringLiteral("app-deactivate");
    case QEvent::ApplicationStateChange: return QStringLiteral("app-state");
    default: return QString::number(int(type));
    }
}
// GUI-thread only. Exact opt-in; repeated states are suppressed, moving samples
// limited to four per second/channel. No timers, IO, process or widget actions.
class Diagnostics {
public:
    typedef std::function<void(const QString &)> Log;
    Diagnostics() : enabled(qgetenv("VLC_TIMELINE_HOVER_DEBUG") == "1") { clock.start(); }
    bool isEnabled() const { return enabled && bool(log); }
    void setLogger(Log logger) { log = logger; }
    void record(const QString &channel, const QString &message, int intervalMs = 250) const {
        if(!isEnabled()) return;
        auto it = last.find(channel);
        const qint64 now = clock.elapsed();
        if(it != last.end() && (it->message == message || now - it->at < intervalMs)) return;
        last[channel] = Entry{message, now};
        log(message);
    }
private:
    struct Entry { QString message; qint64 at; };
    const bool enabled;
    Log log;
    QElapsedTimer clock;
    mutable QMap<QString, Entry> last;
};
}
#endif
