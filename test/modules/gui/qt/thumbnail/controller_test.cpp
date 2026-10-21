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
#include <QElapsedTimer>
#include <QEvent>
#include <cstdio>
#include <cstdlib>
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
        Cache cache(2,1024);QImage small(8,8,QImage::Format_ARGB32);small.fill(Qt::red);
        cache.put(QStringLiteral("a"),small);cache.put(QStringLiteral("b"),small);
        assert(!cache.get(QStringLiteral("a")).isNull());cache.put(QStringLiteral("c"),small);
        assert(cache.get(QStringLiteral("b")).isNull() && cache.count()==2 && cache.bytes()==512);
        Cache bytes(32,256);bytes.put(QStringLiteral("a"),small);bytes.put(QStringLiteral("b"),small);
        assert(bytes.count()==1 && bytes.bytes()==256);
        QImage huge(321,180,QImage::Format_ARGB32);cache.put(QStringLiteral("oversized"),huge);
        cache.put(QString(65,QLatin1Char('x')),small);
        assert(cache.count()==2);cache.clear();assert(cache.bytes()==0);
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
        std::puts("Qt preview mapping/debounce/stale/cache/cancellation/lifecycle tests passed");
    }
};
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);if(argc!=2)return 2;
    TimelinePreview::PreviewTests::run(QString::fromLocal8Bit(argv[1]));
}
