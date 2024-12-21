// "RTSPServer" 的子类，根据指定的流名称是否作为文件存在，按需创建 "ServerMediaSession"
// 实现部分

#include <string.h>

#include <liveMedia.hh>

#include <QString>
#include <QStringList>
#include <QProcess>
#include <QFileInfo>

#include "seekrtspserver.h"
#include "seekh264videofileservermediasubsession.h"

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
            // 先生成264
            QString commandQStr = QString("ffmpeg -i ./%1 -c:v copy -an -bsf h264_mp4toannexb -f h264 -y ./%2").arg(fileName).arg(outFileName);

            QProcess process;
            process.start(commandQStr);
            process.waitForFinished(-1); // -1 表示等待直到命令执行完成
            int exitCode = process.exitCode();
            if (exitCode != 0)
            {
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
