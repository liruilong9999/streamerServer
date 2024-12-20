#include <BasicUsageEnvironment.hh>
#include "seekrtspserver.h"
#include "httpfileserver.h"

#include <QtConcurrent>
#include <QCoreApplication>
/*
请求方法：
1.视频(去掉)
rtsp://127.0.0.1/vedio/xxx.mp4

2.获取文件列表
http://127.0.0.1:8080/filelist
*/

int main(int argc, char ** argv)
{
    QCoreApplication a(argc, argv);
    HttpFileServer   server;
    server.startServer(); // 启动服务器
    QtConcurrent::run([&]() {
        // 开始设置使用环境：
        TaskScheduler *    scheduler = BasicTaskScheduler::createNew();              // 创建任务调度器
        UsageEnvironment * env       = BasicUsageEnvironment::createNew(*scheduler); // 创建使用环境

        UserAuthenticationDatabase * authDB = NULL; // 初始化用户认证数据库为空
#ifdef ACCESS_CONTROL
        // 如果需要实现客户端访问控制到 RTSP 服务器，执行以下操作：
        authDB = new UserAuthenticationDatabase;         // 创建一个新的用户认证数据库
        authDB->addUserRecord("username1", "password1"); // 添加用户记录，用户名为 "username1"，密码为 "password1"
                                                         // 重复上述操作以允许其他 <用户名> 和 <密码> 访问服务器
#endif

        // 创建 RTSP 服务器。首先尝试使用默认端口号（554），
        // 如果失败则使用备用端口号（8554）：
        RTSPServer * rtspServer;
        portNumBits  rtspServerPortNum = 554;                                                           // 默认端口号 554
        rtspServer                     = SeekRTSPServer::createNew(*env, rtspServerPortNum, authDB); // 创建动态 RTSP 服务器
        if (rtspServer == NULL)
        {
            rtspServerPortNum = 8554;                                                          // 如果默认端口号创建失败，使用备用端口号 8554
            rtspServer        = SeekRTSPServer::createNew(*env, rtspServerPortNum, authDB); // 再次尝试创建 RTSP 服务器
        }
        if (rtspServer == NULL)
        {
            *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n"; // 如果创建失败，输出错误信息
            exit(1);                                                                 // 退出程序
        }

        char * urlPrefix = rtspServer->rtspURLPrefix(); // 获取 RTSP 服务器的 URL 前缀
        *env << "rtsp URL: " << urlPrefix << "<fileName>\n";      // 提供支持的文件类型以及文档链接

        // 另外，尝试创建一个 HTTP 服务器以支持 RTSP-over-HTTP 隧道传输。
        // 首先尝试使用默认 HTTP 端口（80），然后尝试备用的 HTTP
        // 端口号（8000 和 8080）。
        if (rtspServer->setUpTunnelingOverHTTP(80) || rtspServer->setUpTunnelingOverHTTP(8000) || rtspServer->setUpTunnelingOverHTTP(8080))
        {
            // *env << "(Use port " << rtspServer->httpServerPortNum() << " for optional RTSP-over-HTTP tunneling, or for HTTP live streaming (for indexed Transport Stream files only).)\n";
        }
        else
        {
            // *env << "(RTSP-over-HTTP tunneling is not available.)\n"; // 如果 RTSP-over-HTTP 隧道传输不可用，输出提示信息
        }

        env->taskScheduler().doEventLoop(); // 进入事件循环，服务器开始工作
    });

    return a.exec();
}
