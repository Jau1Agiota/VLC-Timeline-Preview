/* Optional real-helper protocol/GUI-event-loop test, not PTS/device acceptance.
 * GPL-2.0-or-later. Native-host executable adapter and local media required. */
#include "modules/gui/qt/util/thumbnailpreview.hpp"
#include <QApplication>
#include <QWidget>
#include <QThread>
#include <QElapsedTimer>
#include <cstdio>
#include <iostream>
int main(int argc,char **argv) {
    QApplication app(argc,argv);if(argc!=4)return 2;
    QWidget host;host.resize(800,600);host.show();app.processEvents();
    QImage result;int pulses=0;QTimer heartbeat;heartbeat.setInterval(10);
    QObject::connect(&heartbeat,&QTimer::timeout,[&]{++pulses;});heartbeat.start();
    TimelinePreview::Controller control(&host,[](const QString &){return true;},
        [&](const QImage &image,qint64){result=image;},
        [](const QString &message){std::fprintf(stderr,"%s\n",qPrintable(message));},
        QString::fromLocal8Bit(argv[1]),150,6500);
    QElapsedTimer clock;clock.start();
    control.hover(QString::fromLocal8Bit(argv[2]),QStringLiteral("real-helper-session"),14750);
    while(result.isNull() && clock.elapsed()<10000) { app.processEvents();QThread::msleep(1); }
    if(result.isNull() || pulses<5 || !result.save(QString::fromLocal8Bit(argv[3])))return 3;
    std::cout << "real-helper candidate " << result.width() << 'x' << result.height()
              << "; elapsed=" << clock.elapsed() << " ms; GUI heartbeat=" << pulses << "; PTS unverified\n";
}
