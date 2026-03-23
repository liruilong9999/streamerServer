#include <videocommon/videocapturemanager.h>

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

int main(int argc, char * argv[])
{
    QCoreApplication app(argc, argv);

    const QString  rtspUrl            = "rtsp://127.0.0.1:5541/stream/V4";
    const QString  saveDir            = "video/capture";
    const unsigned segmentDurationSec = 11;  // 每个文件保存 60 秒
    const unsigned diskThresholdGB    = 10;  // 剩余空间低于 10GB 时清理旧文件
    const unsigned targetDurationSec  = 40; // 总录制目标时长 170 秒

    VideoCaptureManager captureManager;

    if (!captureManager.start(rtspUrl, saveDir, segmentDurationSec, diskThresholdGB, targetDurationSec))
    {
        qCritical() << "启动采集失败:" << rtspUrl;
        return -1;
    }

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&captureManager]() {
        captureManager.stop();
    });

    // 示例：达到目标时长后，采集线程会自行结束；这里轮询并退出应用。
    QTimer * exitTimer = new QTimer(&app);
    QObject::connect(exitTimer, &QTimer::timeout, [&captureManager, &app]() {
        if (!captureManager.isRunning())
        {
            app.quit();
        }
    });
    exitTimer->start(1000);

    return app.exec();
}
