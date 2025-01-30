// "RTSPServer" 的子类，根据指定的流名称是否作为文件存在，按需创建 "ServerMediaSession"
// 实现部分

#include <string.h>

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>

#include <QString>
#include <QStringList>
#include <QProcess>
#include <QFileInfo>
#include <QDebug>
#include <QDir>

#include <iostream>

#include <QtConcurrent>

#ifndef _RTSP_SERVER_SUPPORTING_HTTP_STREAMING_HH
#include "RTSPServerSupportingHTTPStreaming.hh"
#endif

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
}

#include "seekrtspserver.h"
#include "seekh264videofileservermediasubsession.h"

// 转换 MP4 文件为 H.264 格式的函数
bool convertMp4ToH264(const std::string & inputPath, const std::string & outputPath)
{
    av_register_all();                           // 注册所有编解码器，输入输出格式等
    AVFormatContext * inputFormatContext = NULL; // 输入文件的格式上下文
    AVPacket          pkt;                       // 存储从输入文件中读取的每一帧数据
    AVCodecContext *  videoCodecContext;         // 视频流的解码上下文
    AVCodec *         videoCodec;                // 视频解码器
    int               ret;
    unsigned          i;
    int               videoStreamIndex = -1;                               // 视频流的索引
    FILE *            outputFile       = fopen(outputPath.c_str(), "wb+"); // 输出文件的指针，打开文件进行写入

    unsigned char * h264Buf; // 用于存储转换后的 H.264 数据
    int             h264Len; // H.264 数据的长度

    // 打开输入文件
    if ((ret = avformat_open_input(&inputFormatContext, inputPath.c_str(), 0, 0)) < 0)
    {
        std::cerr << "无法打开输入文件: " << inputPath << std::endl;
        return false; // 返回 false，表示打开文件失败
    }

    // 获取输入文件的流信息（音频、视频等流的基本信息）
    if ((ret = avformat_find_stream_info(inputFormatContext, 0)) < 0)
    {
        std::cerr << "无法获取流信息" << std::endl;
        return false; // 返回 false，表示获取流信息失败
    }

    // 查找视频流
    for (i = 0; i < inputFormatContext->nb_streams; i++)
    {
        if (inputFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex = i; // 如果是视频流，记录其索引
            break;                // 找到视频流后，跳出循环
        }
    }

    if (videoStreamIndex == -1)
    { // 如果没有找到视频流
        std::cerr << "无法找到视频流" << std::endl;
        return false; // 返回 false，表示没有视频流
    }

    // 输出输入文件的基本信息（比如流的格式、类型、编码等）
    // av_dump_format(inputFormatContext, 0, inputPath.c_str(), 0);

    // 获取视频流的解码器上下文
    videoCodecContext = inputFormatContext->streams[videoStreamIndex]->codec;

    // 查找视频解码器
    videoCodec = avcodec_find_decoder(videoCodecContext->codec_id);
    if (videoCodec == NULL)
    { // 如果找不到视频解码器
        std::cerr << "无法找到视频解码器" << std::endl;
        return false; // 返回 false，表示无法找到视频解码器
    }

    // 打开视频解码器
    if (avcodec_open2(videoCodecContext, videoCodec, NULL) < 0)
    {
        std::cerr << "无法打开视频解码器" << std::endl;
        return false; // 返回 false，表示无法打开视频解码器
    }

    AVFrame * frame = av_frame_alloc(); // 分配一个 AVFrame，用来存储解码后的视频帧

    // 创建 H.264 的比特流过滤器
    AVBitStreamFilterContext * h264BitStreamFilter = av_bitstream_filter_init("h264_mp4toannexb");

    // 读取视频帧
    while (av_read_frame(inputFormatContext, &pkt) >= 0)
    {
        if (pkt.stream_index == videoStreamIndex)
        {
            // 如果是视频流的数据包
            // 使用比特流过滤器将 MP4 格式的 H.264 转换为 Annex B 格式
            av_bitstream_filter_filter(h264BitStreamFilter, inputFormatContext->streams[videoStreamIndex]->codec, NULL, &h264Buf, &h264Len, pkt.data, pkt.size, 0);

            // 将转换后的 H.264 数据写入输出文件
            fwrite(h264Buf, 1, h264Len, outputFile);

            // 释放过滤后的 H.264 数据
            av_free(h264Buf);
        }

        // 释放当前的数据包，准备读取下一帧
        av_packet_unref(&pkt); // 使用 av_packet_unref 代替 av_free_packet，后者已经弃用
    }

    // 清理资源
    av_bitstream_filter_close(h264BitStreamFilter); // 关闭比特流过滤器
    fclose(outputFile);                             // 关闭输出文件
    avformat_close_input(&inputFormatContext);      // 关闭输入文件格式上下文
    avcodec_close(videoCodecContext);               // 关闭视频解码器上下文
    av_frame_free(&frame);                          // 释放解码帧

    return true; // 返回 true，表示成功完成转换
}

class RTSPServerSupportingHTTPStreaming;
class SeekRTSPServer : public RTSPServerSupportingHTTPStreaming
{
public:
    static SeekRTSPServer * createNew(UsageEnvironment & env, Port ourPort, UserAuthenticationDatabase * authDatabase, unsigned reclamationTestSeconds = 65);

protected:
    SeekRTSPServer(UsageEnvironment & env, int ourSocket, Port ourPort, UserAuthenticationDatabase * authDatabase, unsigned reclamationTestSeconds);
    // called only by createNew();
    virtual ~SeekRTSPServer();

protected: // redefined virtual functions
    virtual ServerMediaSession *
    lookupServerMediaSession(char const * streamName, Boolean isFirstLookupInSession);
};

SeekRTSPServer *
SeekRTSPServer::createNew(UsageEnvironment & env, Port ourPort, UserAuthenticationDatabase * authDatabase, unsigned reclamationTestSeconds)
{
    int ourSocket = setUpOurSocket(env, ourPort); // 设置我们的套接字
    if (ourSocket == -1)
        return NULL;

    return new SeekRTSPServer(env, ourSocket, ourPort, authDatabase, reclamationTestSeconds); // 创建新的动态 RTSP 服务器
}

SeekRTSPServer::SeekRTSPServer(UsageEnvironment & env, int ourSocket, Port ourPort, UserAuthenticationDatabase * authDatabase, unsigned reclamationTestSeconds)
    : RTSPServerSupportingHTTPStreaming(env, ourSocket, ourPort, authDatabase, reclamationTestSeconds)
{
}

SeekRTSPServer::~SeekRTSPServer()
{
}

static ServerMediaSession * createNewSMS(UsageEnvironment & env,
                                         char const *       fileName,
                                         FILE *             fid); // 向前声明

ServerMediaSession * SeekRTSPServer::lookupServerMediaSession(char const * streamName, Boolean isFirstLookupInSession)
{
    // 首先，检查指定的 "streamName" 是否作为本地文件存在：
    FILE *  fid        = fopen(streamName, "rb");
    Boolean fileExists = fid != NULL;

    // 接下来，检查是否已经为该文件存在一个 "ServerMediaSession"：
    ServerMediaSession * sms       = RTSPServer::lookupServerMediaSession(streamName);
    Boolean              smsExists = sms != NULL;

    // 处理 "fileExists" 和 "smsExists" 四种可能的情况：
    if (!fileExists)
    {
        if (smsExists)
        {
            // "sms" 是为一个不再存在的文件创建的，移除它：
            removeServerMediaSession(sms);
            sms = NULL;
        }
        return NULL;
    }
    else
    {
        if (smsExists && isFirstLookupInSession)
        {
            // 如果文件发生了变化，移除现有的 "ServerMediaSession" 并创建一个新的：
            removeServerMediaSession(sms);
            sms = NULL;
        }

        if (sms == NULL)
        {
            sms = createNewSMS(envir(), streamName, fid); // 创建新的 ServerMediaSession
            addServerMediaSession(sms);                   // 添加到服务器的媒体会话中
        }

        fclose(fid); // 关闭文件
        return sms;
    }
}

// 处理 Matroska 文件的特殊代码：
struct MatroskaDemuxCreationState
{
    MatroskaFileServerDemux * demux;
    char                      watchVariable;
};
static void onMatroskaDemuxCreation(MatroskaFileServerDemux * newDemux, void * clientData)
{
    MatroskaDemuxCreationState * creationState = (MatroskaDemuxCreationState *)clientData;
    creationState->demux                       = newDemux;
    creationState->watchVariable               = 1;
}
// 结束处理 Matroska 文件的特殊代码：

// 处理 Ogg 文件的特殊代码：
struct OggDemuxCreationState
{
    OggFileServerDemux * demux;
    char                 watchVariable;
};
static void onOggDemuxCreation(OggFileServerDemux * newDemux, void * clientData)
{
    OggDemuxCreationState * creationState = (OggDemuxCreationState *)clientData;
    creationState->demux                  = newDemux;
    creationState->watchVariable          = 1;
}
// 结束处理 Ogg 文件的特殊代码：

#define NEW_SMS(description)                                                   \
    do                                                                         \
    {                                                                          \
        char const * descStr = description                                     \
            ", streamed by the LIVE555 Media Server";                          \
        sms = ServerMediaSession::createNew(env, fileName, fileName, descStr); \
    } while (0)

static ServerMediaSession * createNewSMS(UsageEnvironment & env,
                                         char const *       fileName,
                                         FILE * /*fid*/)
{
    // 使用文件名的扩展名来确定 "ServerMediaSession" 的类型：
    char const * extension = strrchr(fileName, '.');
    if (extension == NULL)
        return NULL;

    ServerMediaSession * sms         = NULL;
    Boolean const        reuseSource = False;
    if (strcmp(extension, ".aac") == 0)
    {
        // 假设为 AAC 音频（ADTS 格式）文件：
        NEW_SMS("AAC Audio");
        sms->addSubsession(ADTSAudioFileServerMediaSubsession::createNew(env, fileName, reuseSource));
    }
    else if (strcmp(extension, ".amr") == 0)
    {
        // 假设为 AMR 音频文件：
        NEW_SMS("AMR Audio");
        sms->addSubsession(AMRAudioFileServerMediaSubsession::createNew(env, fileName, reuseSource));
    }
    else if (strcmp(extension, ".ac3") == 0)
    {
        // 假设为 AC-3 音频文件：
        NEW_SMS("AC-3 Audio");
        sms->addSubsession(AC3AudioFileServerMediaSubsession::createNew(env, fileName, reuseSource));
    }
    else if (strcmp(extension, ".264") == 0)
    {
        // 假设为 H.264 视频元素流文件：
        NEW_SMS("H.264 Video");
        OutPacketBuffer::maxSize = 1000000; // 允许较大的 H.264 帧
        sms->addSubsession(SeekH264VideoFileServerMediaSubsession::createNew(env, fileName, fileName, reuseSource));
    }
    else if (strcmp(extension, ".mp4") == 0)
    {
        QString     outFileName;
        QStringList fileNameList = QString(fileName).split(".");
        if (fileNameList.size() != 2)
        {
            return sms;
        }
        outFileName            = fileNameList[0] + ".264";
        QStringList outDirList = outFileName.split("/");
        if (outDirList.size() != 2)
        {
            return sms;
        }
        outFileName = outDirList[0] + "/outdir/" + outDirList[1];

        QFileInfo fileInfo(outFileName);
        if (fileInfo.exists() && fileInfo.isFile())
        {
        }
        else
        {
            //// 先生成264
            // QString commandQStr = QString("ffmpeg -i ./%1 -c:v copy -an -bsf h264_mp4toannexb -f h264 -y ./%2").arg(fileName).arg(outFileName);

            // QProcess process;
            // process.start(commandQStr);
            // process.waitForFinished(-1); // -1 表示等待直到命令执行完成
            // int exitCode = process.exitCode();
            // if (exitCode != 0)
            // {
            //     return sms;
            //  }
            // 创建输出目录
            // 定义目标路径
            QString path = outDirList[0] + "/outdir";

            // 创建 QDir 对象
            QDir dir;

            // 检查路径是否存在
            if (!dir.exists(path))
            {
                // 如果路径不存在，尝试创建
                if (dir.mkpath(path))
                {
                    qDebug() << "目录创建成功:" << path;
                }
                else
                {
                    qDebug() << "目录创建失败:" << path;
                }
            }
            else
            {
                // qDebug() << "目录已存在:" << path;
            }
            int  t1  = clock();
            bool res = convertMp4ToH264(fileName, outFileName.toStdString());
            int  t2  = clock();
            qDebug() << "MP4转264耗时：" << t2 - t1;
            if (!res)
            {
                qDebug() << "MP4转264失败！！！";
                return sms;
            }
        }

        // 假设为 H.264 视频元素流文件：
        NEW_SMS("mp4 Video");
        OutPacketBuffer::maxSize = 1000000; // 允许较大的 H.264 帧
        sms->addSubsession(SeekH264VideoFileServerMediaSubsession::createNew(env, outFileName.toStdString().c_str(), fileName, reuseSource));
    }
    return sms;
}

RtspServiceManager::RtspServiceManager()
    : m_scheduler(nullptr)
    , m_env(nullptr)
    , m_rtspServer(nullptr)
    , m_authDB(nullptr)
{}

RtspServiceManager::~RtspServiceManager()
{
    stopRtspServer(); // 确保销毁时停止服务
}

void RtspServiceManager::startRtspServer()
{
    if (m_running) // 如果服务器已启动，则直接返回
    {
        qDebug() << "RTSP server is already running.";
        return;
    }

    // 初始化调度器和环境
    m_scheduler = BasicTaskScheduler::createNew();
    m_env       = BasicUsageEnvironment::createNew(*m_scheduler);

#ifdef ACCESS_CONTROL
    // 设置用户认证（可选）
    m_authDB = new UserAuthenticationDatabase;
    m_authDB->addUserRecord("username1", "password1");
#endif

    // 尝试创建 RTSP 服务器
    portNumBits rtspServerPortNum = 554;
    m_rtspServer                  = SeekRTSPServer::createNew(*m_env, rtspServerPortNum, m_authDB);
    if (!m_rtspServer)
    {
        rtspServerPortNum = 8554; // 使用备用端口号
        m_rtspServer      = SeekRTSPServer::createNew(*m_env, rtspServerPortNum, m_authDB);
    }
    if (!m_rtspServer)
    {
        qCritical() << "Failed to create RTSP server:" << m_env->getResultMsg();
        stopRtspServer();
        return;
    }

    // 配置 RTSP-over-HTTP 隧道（可选）
    if (!m_rtspServer->setUpTunnelingOverHTTP(80) && !m_rtspServer->setUpTunnelingOverHTTP(8000) &&
        !m_rtspServer->setUpTunnelingOverHTTP(8080))
    {
        qWarning() << "RTSP-over-HTTP tunneling is not available.";
    }

    m_running = true;

    // 启动事件循环
    QtConcurrent::run([this]() {
        while (m_running)
        {
            m_scheduler->doEventLoop(); // 手动逐步调度任务
        }
    });

    qDebug() << "RTSP server started on port:" << rtspServerPortNum;
}

void RtspServiceManager::stopRtspServer()
{
    if (!m_running)
    {
        qDebug() << "RTSP server is not running.";
        return;
    }

    m_running = false; // 设置标志位，停止事件循环

    // 释放 RTSP 服务器资源
    Medium::close(m_rtspServer);
    m_rtspServer = nullptr;

    // 清理用户认证数据库
    delete m_authDB;
    m_authDB = nullptr;

    // 释放环境和调度器资源
    m_env->reclaim();
    m_env = nullptr;

    delete m_scheduler;
    m_scheduler = nullptr;

    qDebug() << "RTSP server stopped.";
}
