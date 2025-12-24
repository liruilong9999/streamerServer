
#include <videocommon/videocaptureManager.h>
#include <QCoreApplication>

int main(int argc, char * argv[])
{
    QCoreApplication app(argc, argv);

    QString url2 = "rtsp://127.0.0.1/video/test4.mp4";

    VideoCaptureManager captureManager;
    captureManager.startCapture(url2);

    return app.exec();
}
