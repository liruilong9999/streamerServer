#include <videocommon/seekrtspserver.h>
#include <videocommon/httpfileserver.h>

#include <QCoreApplication>
#include <QTimer>
int main(int argc, char ** argv)
{
    QCoreApplication a(argc, argv);

    HttpFileServer server;
    server.startServer(); // 启动 HTTP 文件服务器

    RtspServiceManager rtspManager;
    rtspManager.startRtspServer(); // 启动 RTSP 服务

    // 模拟运行一段时间后停止服务（例如：5分钟后停止）
    QTimer::singleShot(300000, [&rtspManager]() {
        rtspManager.stopRtspServer();
    });
    return a.exec();
}
