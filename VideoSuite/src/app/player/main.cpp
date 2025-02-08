
#include <videocommon/playerwindow.h>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    PlayerWindow playerWindow(true);

    playerWindow.openUrl("rtsp://127.0.0.1/video/test4.mp4");
    playerWindow.show();


    return app.exec();
}