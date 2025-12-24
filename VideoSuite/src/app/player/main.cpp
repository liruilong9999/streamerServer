
#include <videocommon/playerwindow.h>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    PlayerWindow playerWindow(true);

    playerWindow.openUrl("rtsp://admin:wo8023niy@192.168.1.64:554");
    playerWindow.show();


    return app.exec();
}