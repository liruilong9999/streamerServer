#include <QCoreApplication>
#include "httpfileserver.h"

/*
请求方法：
1.视频
http://127.0.0.1:8080/xxx.mp4

2.获取文件列表
http://127.0.0.1:8080/filelist
*/

int main(int argc, char * argv[])
{
    QCoreApplication a(argc, argv);

    // 创建服务器实例
    HttpFileServer server;
    server.startServer(); // 启动服务器

    return a.exec(); // 启动事件循环
}
