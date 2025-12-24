
#include <videocommon/playerwindow.h>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    PlayerWindow playerWindow(true);

    playerWindow.openUrl("rtmp://127.0.0.1:1936/stream/V4");
    playerWindow.show();


    return app.exec();
}