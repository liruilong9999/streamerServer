#ifndef VIDEOCOMMON_H
#define VIDEOCOMMON_H

#include <string>
#include <QWidget>

#include "videocommon_global.h"

// 1.使用RTSP服务（含Http获取文件列表）示例
/*
#include <videocommon/seekrtspserver.h>
#include <videocommon/httpfileserver.h>

#include <QCoreApplication>
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
请求文件列表方式如下
http://127.0.0.1:8082/filelist
返回json
{
    "files": [
        {
            "children": [
                {
                    "isDir": false,
                    "name": "test4.264"
                }
            ],
            "isDir": true,
            "name": "outdir"
        },
        {
            "isDir": false,
            "name": "test4.mp4"
        }
    ]
}
isDir为true，表示文件夹，否则为文件

*/

// 2.使用视频播放器示例
/*
#include <videocommon/playerwindow.h>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    PlayerWindow playerWindow(true);

    playerWindow.openUrl("rtsp://127.0.0.1/vedio/test4.mp4");
    playerWindow.show();

    return app.exec();
}
*/

// 3.使用视频采集器示例
/*
#include <videocommon/VideoCaptureManager.h>
#include <QApplication>

int main(int argc, char * argv[])
{
    QApplication app(argc, argv);

    QString url2 = "rtsp://127.0.0.1/vedio/test4.mp4";

    VideoCaptureManager captureManager;
    captureManager.startCapture(url2);

    return app.exec();
}
*/

#endif // VIDEOCOMMON_H