#include <videocommon/videocapturemanager.h>

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

int main(int argc, char * argv[])
{
    QCoreApplication app(argc, argv);

    // 录制参数示例：RTSP 地址、保存目录、分段时长、磁盘阈值、目标总时长。
    const QString  rtspUrl            = "rtsp://127.0.0.1:5541/stream/V4";
    const QString  saveDir            = "video/capture";
    const unsigned segmentDurationSec = 11; // 每段文件时长（秒）
    const unsigned diskThresholdGB    = 10; // 磁盘剩余阈值（GB）
    const unsigned targetDurationSec  = 40; // 总录制时长（秒），0 表示不限时长

    // 创建 VideoCaptureManager 并启动录制线程。
    VideoCaptureManager captureManager;
    if (!captureManager.start(rtspUrl, saveDir, segmentDurationSec, diskThresholdGB, targetDurationSec))
    {
        qCritical() << "启动采集失败:" << rtspUrl;
        return -1;
    }

    // 应用退出前先停止采集，保证 MP4 文件正常写尾。
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&captureManager]() {
        captureManager.stop();
    });

    // 采集线程结束后自动退出应用（用于命令行录制场景）。
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
