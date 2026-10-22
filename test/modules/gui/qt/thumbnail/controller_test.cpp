/* Real Qt event-loop/subprocess regression tests. GPL-2.0-or-later. */
#include "modules/gui/qt/util/thumbnailpreview.hpp"
#include "modules/gui/qt/util/seekhover.hpp"
#include "modules/gui/qt/util/timetooltip.hpp"
#include <QApplication>
#include <QWidget>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QWindow>
#include <QElapsedTimer>
#include <QEvent>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

namespace TimelinePreview {
class PreviewTests {
    static void spin(int ms) {
        QElapsedTimer clock;clock.start();
        while(clock.elapsed()<ms) { qApp->processEvents();QThread::msleep(1); }
    }
    static bool until(std::function<bool()> condition,int ms=2000) {
        QElapsedTimer clock;clock.start();
        while(!condition() && clock.elapsed()<ms) { qApp->processEvents();QThread::msleep(1); }
        return condition();
    }
public:
    static void run(const QString &helper) {
        assert(bucket(0)==0 && bucket(249)==0 && bucket(250)==250);
        assert(bucket(-1)==-1 && bucket(86400001)==-1);
        const auto point=SeekHover::map(160,9,320,18,9,311,30021,true);
        assert(bucket(point.timestampMs)==15000);
        const QRect bounds(100,50,800,600);
        assert(popupRect(QSize(320,200),QPoint(101,600),bounds).left()==100);
        assert(popupRect(QSize(320,200),QPoint(899,600),bounds).right()==899);
        assert(!popupRect(QSize(320,200),QPoint(400,100),bounds).isValid());
        // Exercise the actual tooltip's image painting and edge/fallback layout.
        TimeTooltip tooltip;
        QImage preview(320,180,QImage::Format_ARGB32);preview.fill(qRgb(20,80,200));
        const QRect window(0,0,800,600);
        tooltip.setTip(QPoint(5,550),QStringLiteral("00:01"),QString(500,QLatin1Char('x')));
        tooltip.setPreview(preview,1000,window);
        assert(tooltip.geometry().left()>=0 && tooltip.geometry().right()<800 && tooltip.height()>180);
        assert(tooltip.width()<=330 && tooltip.geometry().bottom()<550);
        const QImage rendered=tooltip.grab().toImage();
        assert(rendered.pixel(tooltip.width()/2,50)==qRgb(20,80,200));
        tooltip.setTip(QPoint(795,550),QStringLiteral("00:01"),QString());
        assert(tooltip.geometry().right()<800);
        tooltip.setPreview(QImage(),-1,QRect());assert(tooltip.height()<100);
        tooltip.setTip(QPoint(300,20),QStringLiteral("00:01"),QString());
        tooltip.setPreview(preview,1000,window);assert(tooltip.height()<100);
        // Stage 2.4.4: the actual fullscreen controller is a distinct top-level
        // widget; ordinary small windows must never acquire fullscreen bounds.
        QWidget ordinaryOwner, fullscreenOwner;
        assert(popupContext(&ordinaryOwner, &fullscreenOwner) == PopupContext::Windowed);
        assert(popupContext(&fullscreenOwner, &fullscreenOwner) == PopupContext::FullscreenController);
        assert(popupContext(nullptr, nullptr) == PopupContext::Windowed);
        const QRect monitor(0,0,1920,1040), toolbar(562,1013,800,64);
        const QSize imageSize(326,210);
        const QPoint hover(1000,1019);
        assert(!popupRect(imageSize,hover,popupBounds(PopupContext::Windowed,toolbar,monitor)).isValid());
        const QRect fullBounds = popupBounds(PopupContext::FullscreenController,toolbar,monitor);
        const QRect full = popupRect(imageSize,hover,fullBounds,PopupContext::FullscreenController);
        assert(monitor.contains(full) && full.bottom() < toolbar.top());
        const QRect negativeMonitor(-1920,-1080,1920,1040);
        for(const QRect &screen : {monitor, negativeMonitor}) {
            for(int x : {screen.left(),screen.right()}) {
                for(int y : {screen.top()+216, screen.bottom(), screen.bottom()+40}) {
                    const QRect placed = popupRect(imageSize,QPoint(x,y),screen,PopupContext::FullscreenController);
                    assert(placed.isValid() && screen.contains(placed) && placed.bottom()<y);
                }
            }
            assert(!popupRect(imageSize,QPoint(screen.center().x(),screen.top()+100),screen,
                              PopupContext::FullscreenController).isValid());
            assert(!popupRect(QSize(screen.width()+1,100),screen.center(),screen,
                              PopupContext::FullscreenController).isValid());
        }
        assert(!popupRect(imageSize,hover,QRect(),PopupContext::FullscreenController).isValid());
        assert(popupBounds(PopupContext::Windowed,bounds,monitor)==bounds.intersected(monitor));
        // Real tooltip context/transient ownership, not a claim about Windows z-order.
        fullscreenOwner.resize(800,64); fullscreenOwner.show();
        TimeTooltip fullscreenTip;
        const Qt::WindowFlags originalFlags = fullscreenTip.windowFlags();
        fullscreenTip.setPlacementContext(&fullscreenOwner, PopupContext::FullscreenController);
        fullscreenTip.setTip(QPoint(300,550),QStringLiteral("00:01"),QString());
        fullscreenTip.setPreview(preview,1000,QRect(0,530,800,64));
        fullscreenTip.show();spin(5);
        assert(fullscreenTip.height()>180 && fullscreenTip.geometry().bottom()<550);
        assert(fullscreenTip.testAttribute(Qt::WA_ShowWithoutActivating));
        assert(fullscreenTip.windowHandle()->transientParent()==fullscreenOwner.windowHandle());
        assert(fullscreenTip.parentWidget()==nullptr && fullscreenTip.windowFlags()==originalFlags);
        fullscreenTip.setPreview(QImage(),-1,QRect());
        assert(fullscreenTip.height()<100 && fullscreenTip.isVisible());
        fullscreenTip.setPlacementContext(&ordinaryOwner,PopupContext::Windowed);
        assert(!fullscreenTip.testAttribute(Qt::WA_ShowWithoutActivating));
        assert(fullscreenTip.windowHandle()->transientParent()==nullptr);
        assert(fullscreenTip.windowFlags()==originalFlags);
        fullscreenTip.hide();fullscreenOwner.hide();spin(5);
        Cache cache(2,1024);QImage small(8,8,QImage::Format_ARGB32);small.fill(Qt::red);
        cache.put(QStringLiteral("a"),small);cache.put(QStringLiteral("b"),small);
        assert(!cache.get(QStringLiteral("a")).isNull());cache.put(QStringLiteral("c"),small);
        assert(cache.get(QStringLiteral("b")).isNull() && cache.count()==2 && cache.bytes()==512);
        Cache bytes(32,256);bytes.put(QStringLiteral("a"),small);bytes.put(QStringLiteral("b"),small);
        assert(bytes.count()==1 && bytes.bytes()==256);
        QImage huge(321,180,QImage::Format_ARGB32);cache.put(QStringLiteral("oversized"),huge);
        cache.put(QString(65,QLatin1Char('x')),small);
        assert(cache.count()==2);cache.clear();assert(cache.bytes()==0);
        // Default shared budget: exactly 512 LRU entries, independently byte bounded.
        Cache large;
        QImage frame(320,180,QImage::Format_ARGB32);frame.fill(Qt::green);
        for(int i=0;i<513;++i) large.put(QString::number(i),frame);
        assert(large.count()==512 && large.bytes()==qint64(512)*320*180*4);
        assert(large.get(QStringLiteral("0")).isNull());
        large.put(QStringLiteral("512"),small);
        assert(large.count()==512 && large.bytes()==qint64(511)*320*180*4+256);
        Cache strict(512,320*180*4);
        strict.put(QStringLiteral("a"),frame);strict.put(QStringLiteral("b"),frame);
        assert(strict.count()==1 && strict.bytes()==320*180*4);
        QTemporaryDir inputs; assert(inputs.isValid());
        auto file=[&](const char *name) { QString path=inputs.path()+QLatin1Char('/')+QString::fromLatin1(name);
            QFile f(path);assert(f.open(QIODevice::WriteOnly));f.write("test");return path; };
        const QString normal=file("ordinary.mp4"),slow=file("slow.mkv"),error=file("error.mp4"),
                      timeout=file("timeout.mkv"),spam=file("spam.mp4");
        QWidget host;host.resize(800,600);host.show();spin(5);
        bool eligible=true;QString session=QStringLiteral("session1");
        QList<qint64> published;QImage last;
        Controller control(&host,[&](const QString &token) { return eligible && token==session; },
            [&](const QImage &image,qint64 time) { last=image;if(!image.isNull())published.append(time); },
            [](const QString &) {},helper,40,700);
        control.hover(normal,session,1000);spin(10);control.hover(normal,session,1200);
        spin(10);control.hover(normal,session,1750);spin(20);
        assert(control.launches==0);assert(until([&]{return !last.isNull();}));
        assert(control.launches==1 && published.last()==1750);
        control.hover(normal,session,1800);spin(80);assert(control.launches==1); // same bucket
        control.cancel();assert(last.isNull());control.hover(normal,session,1800);spin(10);
        assert(control.showing() && control.launches==1); // cache survives leave
        published.clear();control.hover(slow,session,2000);
        assert(until([&]{return control.process.state()==QProcess::Running;}));
        control.hover(slow,session,3000);assert(last.isNull());
        assert(until([&]{return !last.isNull();}));
        assert(published.size()==1 && published.last()==3000); // killed result never paints
        control.hover(slow,session,4000);assert(until([&]{return control.process.state()==QProcess::Running;}));
        control.cancel();spin(500);assert(last.isNull() && !control.pending && !control.debounce.isActive());
        assert(control.process.state()==QProcess::NotRunning && !QFile::exists(control.output));
        control.hover(normal,session,0);assert(until([&]{return !last.isNull();}));
        eligible=false;assert(until([&]{return !control.showing();},500));
        assert(control.cache->count()==0); // stationary cursor on invalid/stopped input
        eligible=true;control.hover(slow,session,1000);
        assert(until([&]{return control.process.state()==QProcess::Running;}));
        session=QStringLiteral("session2");control.cancel(true);spin(450);assert(last.isNull());
        assert(control.cache->count()==0);
        control.hover(normal,session,1000);assert(until([&]{return !last.isNull();}));
        QEvent leave(QEvent::WindowDeactivate);qApp->sendEvent(&host,&leave);
        assert(!control.showing());
        control.hover(error,session,1000);assert(until([&]{return control.failedAt.isValid();}));
        assert(last.isNull());
        control.failedAt.invalidate();control.hover(timeout,session,1000);
        assert(until([&]{return control.failedAt.isValid();}));
        assert(until([&]{return control.process.state()==QProcess::NotRunning;}));
        assert(last.isNull() && !QFile::exists(control.output));
        control.cancel(true);control.hover(spam,session,1000);
        assert(until([&]{return control.failedAt.isValid();}));assert(last.isNull());
        assert(control.stdoutData.size()<=4096);
        control.cancel(true);control.hover(normal,session,1000);assert(until([&]{return !last.isNull();}));
        // File content/metadata change invalidates the old image without media-token change.
        QFile changed(normal);assert(changed.open(QIODevice::Append));changed.write("changed");changed.close();
        const int count=control.launches;control.hover(normal,session,1000);
        assert(last.isNull());assert(until([&]{return !last.isNull();}));assert(control.launches==count+1);
        control.cancel();
        {
            Controller reuse(&host,[&](const QString &){return true;},[](const QImage &,qint64){},
                             [](const QString &){},helper,1,700);
            reuse.hover(normal,session,1000);
            assert(reuse.showing() && reuse.launches==0); // shared cache hit on a second timeline
        }
        control.cancel(true);
        {
            Controller other(&host,[&](const QString &){return true;},[](const QImage &,qint64){},
                             [](const QString &){},helper,1,700);
            control.hover(slow,session,1000);assert(until([&]{return control.process.state()==QProcess::Running;}));
            other.hover(slow,session,2000);assert(until([&]{return other.process.state()==QProcess::Running;}));
            assert(control.process.state()==QProcess::NotRunning && !control.pending);
            assert(other.cache==control.cache);
        }
        // Crash and timeout must not poison later requests or the process slot.
        control.cancel(true);
        control.hover(error,session,9000);assert(until([&]{return control.failedAt.isValid();}));
        control.hover(normal,session,9250);assert(until([&]{return control.showing();}));
        control.failedAt.invalidate();control.hover(timeout,session,9500);assert(until([&]{return control.failedAt.isValid();}));
        assert(until([&]{return control.process.state()==QProcess::NotRunning;}));
        control.hover(normal,session,9750);assert(until([&]{return control.showing();}));
        const QString missing = inputs.path()+QStringLiteral("/missing-helper");
        Controller absent(&host,[&](const QString &){return true;},[](const QImage &,qint64){},
                          [](const QString &){},missing,1,200);
        absent.hover(normal,session,0);assert(until([&]{return absent.failedAt.isValid();}));
        assert(absent.launches==0 && absent.process.state()==QProcess::NotRunning);
        const QString invalid = file("not-a-program");
        QFile::setPermissions(invalid,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
        Controller broken(&host,[&](const QString &){return true;},[](const QImage &,qint64){},
                          [](const QString &){},invalid,1,200);
        broken.hover(normal,session,0);assert(until([&]{return broken.failedAt.isValid();}));
        assert(broken.process.state()==QProcess::NotRunning);
        assert(broken.cache==control.cache); // global, not one cache per timeline
        const QString temp = [&] {
            Controller dying(&host,[&](const QString &){return true;},[](const QImage &,qint64){},
                             [](const QString &){},helper,1,700);
            dying.hover(slow,session,1000);assert(until([&]{return dying.process.state()==QProcess::Running;}));
            return dying.directory->path();
        }();
        assert(!QDir(temp).exists()); // destructor kills/reaps before deleting session files
        // Diagnostics observe the same real controller/tooltip; they never show
        // a hidden tooltip or alter the placement fallback being investigated.
        // Real controller/event-loop timing with a fake decoder. This measures
        // scheduling/cache mechanics only; it is explicitly not a media benchmark.
        std::vector<qint64> cold, cached;
        Controller timing(&host,[](const QString &){return true;},[](const QImage &,qint64){},
                          [](const QString &){},helper);
        assert(timing.debounce.interval()==35);
        timing.cancel(true);
        for(int i=0;i<12;++i) {
            QElapsedTimer elapsed;elapsed.start();
            timing.hover(normal,session,i*250);
            assert(until([&]{return timing.showing();}));cold.push_back(elapsed.elapsed());
            timing.cancel();elapsed.restart();timing.hover(normal,session,i*250);
            assert(timing.showing());cached.push_back(elapsed.elapsed());
        }
        const int before=timing.launches;
        for(int i=0;i<20;++i) { timing.hover(normal,session,10000+i*250);spin(2); }
        assert(until([&]{return timing.showing();}));assert(timing.launches==before+1);
        assert(timing.wanted.time==14750 && timing.running.time==14750);
        auto report=[](const char *name,std::vector<qint64> values) {
            std::sort(values.begin(),values.end());
            std::printf("::notice title=Controller scheduling::fake_decoder=1 workload=%s n=%u median_ms=%g p95_ms=%d\n",
                name,unsigned(values.size()),(values[5]+values[6])/2.0,int(values[11]));
        };
        report("uncached",cold);report("cached",cached);
        timing.cancel(true);
        const QByteArray savedDebug = qgetenv("VLC_TIMELINE_HOVER_DEBUG");
        qunsetenv("VLC_TIMELINE_HOVER_DEBUG");
        QStringList records;
        auto collect = [&](const QString &message) { records.append(message); };
        Diagnostics quiet;quiet.setLogger(collect);
        quiet.record(QStringLiteral("test"),QStringLiteral("must-not-log"),0);
        assert(records.isEmpty());
        qputenv("VLC_TIMELINE_HOVER_DEBUG","true");
        Diagnostics exact;exact.setLogger(collect);
        exact.record(QStringLiteral("test"),QStringLiteral("must-not-log"),0);
        assert(records.isEmpty());
        qputenv("VLC_TIMELINE_HOVER_DEBUG","1");
        Diagnostics trace;trace.setLogger(collect);
        trace.record(QStringLiteral("sample"),QStringLiteral("first"));
        trace.record(QStringLiteral("sample"),QStringLiteral("first"),0);
        trace.record(QStringLiteral("sample"),QStringLiteral("rate-limited"));
        assert(records.size()==1);
        spin(260);trace.record(QStringLiteral("sample"),QStringLiteral("second"));
        assert(records.size()==2);
        const auto contains = [&](const char *text) {
            return records.join(QLatin1Char('\n')).contains(QString::fromLatin1(text));
        };
        records.clear();
        {
            TimeTooltip observed;observed.setDiagnosticLogger(collect);
            observed.setTip(QPoint(300,550),QStringLiteral("00:01"),QString());
            observed.setPreview(preview,1000,window);
            assert(!observed.isVisible()); // setTip/setPreview must not fix visibility
            assert(contains("stage=set-tip") && contains("stage=set-preview"));
            assert(contains("stage=preview-placement") && contains("fallback=0"));
            assert(contains("owner_bounds=0,0,800,600") && contains("screen="));
            assert(contains("stage=placement-final") && contains("final_rect="));
            observed.show();spin(5);assert(observed.isVisible());
            observed.hide();spin(5);assert(!observed.isVisible());
            assert(contains("event=show") && contains("event=hide") && contains("stage=show-call"));
            QEvent deactivate(QEvent::WindowDeactivate);qApp->sendEvent(&observed,&deactivate);
            assert(contains("event=deactivate"));
            observed.setTip(QPoint(300,20),QStringLiteral("00:01"),QString());
            observed.setPreview(preview,1000,window);
            assert(observed.height()<100 && !observed.isVisible());
            assert(contains("fallback=1")); // original timestamp-only fallback retained
        }
        records.clear();
        {
            Controller traced(&host,[](const QString &){return true;},[](const QImage &,qint64){},
                              [](const QString &){},helper,1,700);
            traced.setDiagnosticLogger(collect);
            traced.hover(normal,QStringLiteral("diagnostic-session"),6500);
            assert(until([&]{return traced.showing();}));
            assert(contains("stage=hover") && contains("stage=launch") && contains("stage=spawn"));
            assert(contains("event=cache-miss") && contains("request_to_launch_ms=") && contains("request_to_present_ms="));
            assert(contains("stage=finished") && contains("latest=1") && contains("controller="));
            QEvent deactivate(QEvent::WindowDeactivate);qApp->sendEvent(&host,&deactivate);
            assert(!traced.showing() && contains("stage=activation") && contains("stage=window-event") && contains("stage=cancel"));
        }
        if(savedDebug.isNull()) qunsetenv("VLC_TIMELINE_HOVER_DEBUG");
        else qputenv("VLC_TIMELINE_HOVER_DEBUG",savedDebug);
        std::puts("Qt preview mapping/debounce/stale/cache/cancellation/lifecycle tests passed");
        std::puts("Diagnostics opt-in/rate-limit/controller/visibility/placement tests passed");
    }
};
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);if(argc!=2)return 2;
    TimelinePreview::PreviewTests::run(QString::fromLocal8Bit(argv[1]));
}
