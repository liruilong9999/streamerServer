
#include <videocommon/playerwindow.h>
#include <QApplication>
#include <QTimer>
#include <time.h>
#include <qdebug.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    PlayerWindow playerWindow(true);

    playerWindow.openUrl("rtsp://127.0.0.1:5541/stream/V4");

    //QTimer::singleShot(10000, [&playerWindow]() {
    //    int t1 = clock();
    //    playerWindow.openUrl("rtsp://192.168.33.5:5541/stream/V4");
    //    qDebug() << int(clock() - t1);
    //}
    //);

    playerWindow.show();

    return app.exec();
}
