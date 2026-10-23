/* Real Qt event-loop/subprocess regression tests. GPL-2.0-or-later. */
#include "modules/gui/qt/util/thumbnailpreview.hpp"
#include "modules/gui/qt/util/seekhover.hpp"
#include "modules/gui/qt/util/timetooltip.hpp"
#include "modules/gui/qt/util/playbackdiagnostics.hpp"
#include <QApplication>
#include <QWidget>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QWindow>
#include <QSlider>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionSlider>
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
        // Model VLC SeekStyle's Windows base and 18px handle metric using real
        // Qt subControlRect/sliderValueFromPosition, not a hand-coded rectangle.
        class FixedHandle : public QProxyStyle {
        public:
            FixedHandle():QProxyStyle(QStyleFactory::create(QStringLiteral("Windows"))) {}
            int pixelMetric(PixelMetric m,const QStyleOption *o=nullptr,const QWidget *w=nullptr) const override {
                return m==PM_SliderLength || m==PM_SliderThickness ? 18 : QProxyStyle::pixelMetric(m,o,w);
            }
        };
        QSlider reference(Qt::Horizontal);reference.resize(1280,18);reference.setRange(0,10000);
        FixedHandle *metric=new FixedHandle;metric->setParent(&reference);reference.setStyle(metric);
        QStyleOptionSlider opt;opt.initFrom(&reference);opt.orientation=Qt::Horizontal;
        opt.minimum=0;opt.maximum=10000;opt.sliderPosition=0;opt.sliderValue=0;
        const int a=metric->subControlRect(QStyle::CC_Slider,&opt,QStyle::SC_SliderHandle,&reference).center().x();
        opt.sliderPosition=opt.sliderValue=10000;
        const int b=metric->subControlRect(QStyle::CC_Slider,&opt,QStyle::SC_SliderHandle,&reference).center().x();
        const int clickValue=QStyle::sliderValueFromPosition(0,10000,640-18/2,1280-18,false);
        const qint64 clickTime=SeekHover::clickTimestamp(clickValue,10000,7200000);
        const qint64 oldRequest=bucket(SeekHover::map(640,9,1280,18,a,b,7200000,true).timestampMs);
        assert(clickTime==3600000 && oldRequest-clickTime==5500);
        std::printf("::notice title=Click geometry::width=1280 handle=18 duration_ms=7200000 x=640 style_start=%d style_end=%d fast_ms=%d click_ms=%d delta_ms=%d; host Qt geometry model, not Windows device test\n",a,b,int(oldRequest),int(clickTime),int(oldRequest-clickTime));
        const auto point=SeekHover::map(160,9,320,18,9,311,30021,true);
        assert(bucket(point.timestampMs)==15000);
        // Diagnostic observations must not accuse a paused input, cross media
        // generations, or compare an unavailable/backward/late sample blindly.
        PlaybackStability::Progress progress;
        PlaybackStability::Sample s;s.playing=true;s.available=true;s.timeMs=1000;
        assert(progress.observe(s,1000).contains(QStringLiteral("observation=baseline")));
        s.timeMs=2000;s.audio=24;s.playedAudio=24;
        assert(progress.observe(s,1000).contains(QStringLiteral("observation=audio-progress-video-static")));
        s.timeMs=3000;s.decoded=24;
        assert(progress.observe(s,1000).contains(QStringLiteral("observation=decode-progress-no-display")));
        s.timeMs=4000;s.displayed=24;
        assert(progress.observe(s,1000).contains(QStringLiteral("observation=display-counter-progress")));
        s.playing=false;
        assert(progress.observe(s,1000).contains(QStringLiteral("observation=not-playing")));
        s.playing=true;
        assert(progress.observe(s,1000).contains(QStringLiteral("observation=baseline")));
        s.available=false;
        assert(progress.observe(s,1000).contains(QStringLiteral("observation=stats-unavailable")));
        s.available=true;
        assert(progress.observe(s,1000).contains(QStringLiteral("observation=baseline")));
        s.timeMs=1000;
        assert(progress.observe(s,1000).contains(QStringLiteral("observation=seek-or-counter-reset")));
        progress.reset();
        const QString resetSample=progress.observe(s,4000);
        assert(resetSample.contains(QStringLiteral("observation=baseline")) &&
               resetSample.contains(QStringLiteral("gui_gap=1")));
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
        // Stage D: geometry observers must never see a new position with the
        // previous timestamp-only dimensions during an image handoff.
        class GeometryTooltip : public TimeTooltip {
        public:
            QList<QRect> moved;
            bool event(QEvent *e) override {
                if(e->type()==QEvent::Move) moved.append(geometry());
                return TimeTooltip::event(e);
            }
        } geometryTip;
        geometryTip.setTip(QPoint(400,550),QStringLiteral("00:01"),QString());
        geometryTip.show();spin(5);geometryTip.moved.clear();
        geometryTip.setPreview(preview,1000,window);
        assert(!geometryTip.moved.isEmpty());
        for(const QRect &seen : geometryTip.moved) assert(seen.size()==geometryTip.size());

        geometryTip.moved.clear();
        geometryTip.setPreview(QImage(),-1,QRect());
        assert(!geometryTip.moved.isEmpty() && geometryTip.height()<100);
        for(const QRect &seen : geometryTip.moved) assert(seen.size()==geometryTip.size());
        geometryTip.setPreview(preview,1000,window);

        // Cache/worker updates must use the live timeline top even if no mouse
        // event followed a toolbar/layout move. Fullscreen keeps monitor bounds.
        QWidget anchorOwner;anchorOwner.setGeometry(0,0,800,600);
        QWidget timeline(&anchorOwner);timeline.setGeometry(0,500,800,18);
        geometryTip.setPlacementContext(&anchorOwner,PopupContext::Windowed,&timeline);
        geometryTip.setTip(QPoint(400,500),QStringLiteral("00:01"),QString());
        geometryTip.setPreview(preview,1000,window);
        const int beforeBottom=geometryTip.geometry().bottom();
        timeline.move(0,470);
        geometryTip.setPreview(preview,2000,window); // async/cache result; no setTip
        assert(geometryTip.geometry().bottom()==beforeBottom-30);
        const int anchoredY=geometryTip.y();
        for(int i=0;i<100;++i) {
            geometryTip.setTip(QPoint(i%2 ? 795 : 5,999),QStringLiteral("00:02"),QString());
            geometryTip.setPreview(preview,2000+i,window);
            assert(geometryTip.y()==anchoredY && window.contains(geometryTip.geometry()));
        }
        geometryTip.setPlacementContext(&anchorOwner,PopupContext::FullscreenController,&timeline);
        geometryTip.setPreview(preview,4000,window);
        assert(geometryTip.geometry().bottom()<timeline.mapToGlobal(QPoint()).y());
        const int fullscreenY=geometryTip.y();
        geometryTip.setPreview(preview,5000,window);assert(geometryTip.y()==fullscreenY);
        geometryTip.hide();
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
        assert(until([&]{return control.process.state()==QProcess::Running && control.workerBusy;}));
        control.hover(slow,session,3000);assert(last.isNull());
        assert(until([&]{return !last.isNull();}));
        assert(published.size()==1 && published.last()==3000); // killed result never paints
        control.hover(slow,session,4000);assert(until([&]{return control.process.state()==QProcess::Running && control.workerBusy;}));
        control.cancel();spin(500);assert(last.isNull() && !control.pending && !control.debounce.isActive());
        assert(control.process.state()==QProcess::NotRunning && !QFile::exists(control.output));
        control.hover(normal,session,0);assert(until([&]{return !last.isNull();}));
        eligible=false;assert(until([&]{return !control.showing();},500));
        assert(control.cache->count()==0); // stationary cursor on invalid/stopped input
        eligible=true;control.hover(slow,session,1000);
        assert(until([&]{return control.process.state()==QProcess::Running && control.workerBusy;}));
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
            control.hover(slow,session,1000);assert(until([&]{return control.process.state()==QProcess::Running && control.workerBusy;}));
            other.hover(slow,session,2000);assert(until([&]{return other.process.state()==QProcess::Running && other.workerBusy;}));
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
            dying.hover(slow,session,1000);assert(until([&]{return dying.process.state()==QProcess::Running && dying.workerBusy;}));
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
        std::vector<qint64> oldDebounce;
        Controller oldTiming(&host,[](const QString &){return true;},[](const QImage &,qint64){},
                             [](const QString &){},helper,150);
        oldTiming.cancel(true);
        for(int i=0;i<12;++i) {
            QElapsedTimer elapsed;elapsed.start();oldTiming.hover(normal,session,i*250);
            assert(until([&]{return oldTiming.showing();}));oldDebounce.push_back(elapsed.elapsed());
        }
        report("baseline_150ms_uncached",oldDebounce);oldTiming.cancel(true);
        timing.cancel(true);
        {
            Controller worker(&host,[](const QString &){return true;},[](const QImage &,qint64){},
                              [](const QString &){},helper,1,700);
            worker.cancel(true);worker.hover(normal,session,0);
            assert(until([&]{return worker.showing();}));
            const qint64 pid=worker.process.processId();
            worker.hover(normal,session,250);assert(until([&]{return worker.showing();}));
            assert(worker.workerRequests==2 && worker.process.processId()==pid);
            worker.cancel();assert(worker.process.state()==QProcess::Running && !worker.workerBusy);
            worker.idle.setInterval(20);worker.idle.start();
            assert(until([&]{return worker.process.state()==QProcess::NotRunning;}));
            worker.idle.setInterval(30000);
            worker.hover(normal,session,500);assert(until([&]{return worker.showing();}));
            assert(worker.workerRequests==1);
            worker.workerRequests=63;worker.hover(normal,session,750);
            assert(until([&]{return worker.showing();}));
            assert(until([&]{return worker.process.state()==QProcess::NotRunning;}));
            worker.hover(normal,session,1000);assert(until([&]{return worker.showing();}));
            assert(worker.workerRequests==1);
            const QString bad=file("badid.mp4");worker.failedAt.invalidate();
            worker.hover(bad,session,1250);assert(until([&]{return worker.failedAt.isValid();}));
            assert(!worker.showing());
            worker.hover(normal,session,1500);assert(until([&]{return worker.showing();}));
            const QString unsolicited=file("unsolicited.mp4");worker.failedAt.invalidate();
            worker.hover(unsolicited,session,1750);assert(until([&]{return worker.showing();}));
            assert(until([&]{return worker.failedAt.isValid();}));
            assert(until([&]{return worker.process.state()==QProcess::NotRunning;}));
            worker.hover(normal,session,2000);assert(until([&]{return worker.showing();}));
        }
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
        qputenv("VLC_THUMBNAIL_BACKEND","unknown");
        {
            int errors=0;
            Controller rejected(&host,[](const QString &){return true;},[](const QImage &,qint64){},
                                [&](const QString &){++errors;},helper);
            rejected.hover(normal,session,1000);spin(50);
            assert(errors==1 && rejected.launches==0 && !rejected.showing());
        }
        qputenv("VLC_THUMBNAIL_BACKEND","libmpv");
        const QByteArray savedHelper=qgetenv("VLC_MPV_HELPER");qunsetenv("VLC_MPV_HELPER");
        {
            int errors=0;
            Controller absentMpv(&host,[](const QString &){return true;},[](const QImage &,qint64){},
                                 [&](const QString &){++errors;});
            absentMpv.hover(normal,session,1000);spin(50);
            assert(errors==1 && absentMpv.launches==0 && !absentMpv.showing());
        }
        if(savedHelper.isNull())qunsetenv("VLC_MPV_HELPER");else qputenv("VLC_MPV_HELPER",savedHelper);
        {
            published.clear(); last=QImage();
            Controller mpv(&host,[](const QString &){return true;},
                [&](const QImage &image,qint64 time){last=image;if(!image.isNull())published.append(time);},
                [](const QString &){},helper,1,700);
            assert(mpv.mpvBackend && mpv.debounce.interval()==16);
            mpv.cancel(true);mpv.hover(normal,session,1000);
            assert(until([&]{return mpv.showing();}));
            assert(!mpv.directory && mpv.output.isEmpty());
            const qint64 pid=mpv.process.processId();
            mpv.hover(normal,session,2000);assert(!last.isNull());
            assert(until([&]{return mpv.workerBusy;}));
            mpv.hover(normal,session,3000);mpv.hover(normal,session,9000);
            assert(until([&]{return !published.isEmpty() && published.last()==9000;}));
            assert(published.contains(2000) && !published.contains(3000));
            assert(mpv.process.processId()==pid && mpv.workerRequests==3);
            // Newer cached completion cannot be overwritten by older in-flight output.
            mpv.hover(normal,session,10000);assert(until([&]{return mpv.workerBusy;}));
            mpv.hover(normal,session,1000);const int publications=published.size();spin(150);
            assert(published.size()==publications && published.last()==1000);
            mpv.hover(normal,session,11000);assert(until([&]{return mpv.workerBusy;}));
            mpv.cancel();spin(150);assert(last.isNull() && !mpv.showing());
            assert(mpv.process.state()==QProcess::Running); // leave does not destroy persistent decoder
            mpv.hover(normal,session,12000);assert(until([&]{return mpv.showing();}));
            assert(mpv.process.processId()==pid);
            mpv.hover(normal,session,13000);assert(until([&]{return mpv.workerBusy;}));
            mpv.cancel(true);assert(until([&]{return mpv.process.state()==QProcess::NotRunning;}));
            assert(last.isNull() && mpv.cache->count()==0);
            mpv.hover(error,session,14000);assert(until([&]{return mpv.failedAt.isValid();}));
            assert(!mpv.showing());
            mpv.hover(normal,session,15000);assert(until([&]{return mpv.showing();}));
            const QString bad=file("mpv-badid.mp4");mpv.failedAt.invalidate();
            mpv.hover(bad,session,16000);assert(until([&]{return mpv.failedAt.isValid();}));
            assert(until([&]{return mpv.process.state()==QProcess::NotRunning;}));
            mpv.failedAt.invalidate();mpv.hover(timeout,session,17000);
            assert(until([&]{return mpv.failedAt.isValid();}));
            assert(until([&]{return mpv.process.state()==QProcess::NotRunning;}));
            mpv.hover(normal,session,18000);assert(until([&]{return mpv.showing();}));
            mpv.hover(normal,session,19000);assert(until([&]{return mpv.workerBusy;}));
            mpv.cancel();spin(120);mpv.hover(normal,session,20000);
            assert(until([&]{return mpv.showing();}));const int beforeRapid=mpv.workerRequests;
            for(int i=0;i<20;++i) {
                mpv.hover(normal,session,21000+i*250);spin(5);assert(!last.isNull());
            }
            assert(until([&]{return !published.isEmpty() && published.last()==25750;}));
            assert(mpv.workerRequests-beforeRapid<6); // bounded work, not 20 independent extractions
            mpv.hover(normal,session,27000);assert(until([&]{return mpv.workerBusy;}));
            // Destructor below terminates an active child; bounded existing teardown path.
        }
        qputenv("VLC_MPV_SEEK","exact");
        {
            published.clear();
            Controller exact(&host,[](const QString &){return true;},
                [&](const QImage &image,qint64 time){last=image;if(!image.isNull())published.append(time);},
                [](const QString &){},helper,1,700);
            exact.cancel(true);assert(exact.exactSeeking());
            exact.hover(normal,session,1234);assert(until([&]{return exact.showing();}));
            assert(published.last()==1234); // no 250ms floor
            exact.hover(normal,session,1301);assert(until([&]{return exact.workerBusy;}));
            exact.cancel();exact.hover(normal,session,7891);
            assert(until([&]{return !published.isEmpty() && published.last()==7891;}));
            assert(!published.contains(1301)); // canceled lifecycle may not resurface
            exact.cancel(true);
        }
        qunsetenv("VLC_MPV_SEEK");
        assert(SeekHover::clickTimestamp(4567,10000,7200123)==qint64(7200123.0*(4567/static_cast<float>(10000))+0.5));
        assert(SeekHover::clickTimestamp(0,10000,7200123)==0);
        assert(SeekHover::clickTimestamp(10000,10000,7200123)==7200123);
        assert(SeekHover::clickTimestamp(1,0,7200123)==-1);
        qunsetenv("VLC_THUMBNAIL_BACKEND");
        std::puts("Exact request precision/lifecycle fence/click-fraction mapping tests passed");
        std::puts("libmpv protocol/persistence/latest-pending/intermediate-label/cache-order/leave/crash/timeout/cleanup tests passed");
        std::puts("Qt preview mapping/debounce/stale/cache/cancellation/lifecycle tests passed");
        std::puts("Diagnostics opt-in/rate-limit/controller/visibility/placement tests passed");
    }
};
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);if(argc!=2)return 2;
    TimelinePreview::PreviewTests::run(QString::fromLocal8Bit(argv[1]));
}
