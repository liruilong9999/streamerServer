
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>

#include <QStorageInfo>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/timestamp.h>
#include <libavutil/opt.h>
}

#include "videocaptureManager.h"

#define SETTINGS_PATH "config/autoCapture.ini"

VideoCaptureManager::VideoCaptureManager(QObject * parent)
    : QThread(parent)
    , m_isRunning(false)
{
    QSettings settings(SETTINGS_PATH, QSettings::IniFormat);
    m_segmentDuration = settings.value("Capture/Duration", 60).toInt(); // 单位: 秒
    m_diskThresholdGB = settings.value("Capture/DiskThresholdGB", 10).toInt();
    m_outputDir       = settings.value("Capture/SaveDir", "video").toString();

    QDir dir(m_outputDir);
    if (!dir.exists())
    {
        dir.mkpath(".");
    }
}

VideoCaptureManager::~VideoCaptureManager()
{
    stopCapture();
}

void VideoCaptureManager::startCapture(const QString & inputUrl)
{
    if (m_isRunning)
    {
        qDebug() << "正在进行视频录制，请等待结束后再开始新的录制。";
        return;
    }
    if (!inputUrl.contains("rtsp://"))
    {
        qDebug() << "只能保存网络流媒体文件，不能保存本地文件。";
        return;
    }
    m_inputUrl = inputUrl;

    m_isRunning = true;
    start();
}

void VideoCaptureManager::stopCapture()
{
    if (!m_isRunning)
        return;

    m_isRunning = false;
    wait();
}

void VideoCaptureManager::run()
{
    // 初始化输入格式上下文和选项字典
    AVFormatContext * inputCtx = nullptr;
    AVDictionary *    options  = nullptr;
    AVPacket          packet;

    // 设置打开输入流的选项
    av_dict_set(&options, "stimeout", "10000000", 0);   // 10秒超时
    av_dict_set(&options, "rtsp_transport", "tcp", 0);  // 使用TCP
    av_dict_set(&options, "max_delay", "500000", 0);    // 设置最大延迟
    av_dict_set(&options, "buffer_size", "1024000", 0); // 增加缓冲区大小

    // 打开输入流
    int ret = avformat_open_input(&inputCtx, m_inputUrl.toStdString().c_str(), nullptr, &options);
    if (ret < 0)
    {
        qDebug() << "打开输入流失败:" << m_inputUrl;
        av_dict_free(&options);
        return;
    }
    av_dict_free(&options);

    // 查找流信息
    if (avformat_find_stream_info(inputCtx, nullptr) < 0)
    {
        qDebug() << "查找流信息失败。";
        avformat_close_input(&inputCtx);
        return;
    }

    double fps = 30;
    // 获取视频流的帧率
    for (unsigned int i = 0; i < inputCtx->nb_streams; ++i)
    {
        AVStream * stream = inputCtx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            // 获取视频流的帧率
            fps = av_q2d(stream->avg_frame_rate);
            break;
        }
    }

    // 获取流的帧率并计算目标帧数
    if (fps <= 0)
    {
        qDebug() << "无效的视频流。";
        avformat_close_input(&inputCtx);
        return;
    }

    int     targetFrameCount = static_cast<int>(m_segmentDuration * fps);
    int64_t segmentStartPts  = AV_NOPTS_VALUE; // 记录每段视频的起始 PTS

    while (m_isRunning)
    {
        // 创建新的视频文件名
        QString timestamp  = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        QString outputFile = m_outputDir + "/" + timestamp + ".mp4";
        qDebug() << "输出文件路径为:" << outputFile;

        // 管理输出目录的磁盘空间
        manageDiskSpace(m_outputDir);

        AVFormatContext * outputCtx      = nullptr;
        int               framesCaptured = 0;

        // 分配输出格式上下文
        if (avformat_alloc_output_context2(&outputCtx, nullptr, nullptr, outputFile.toStdString().c_str()) < 0)
        {
            qDebug() << "分配 输出流上下文 失败。";
            avformat_close_input(&inputCtx);
            return;
        }

        // 复制输入流到输出流
        for (unsigned int i = 0; i < inputCtx->nb_streams; ++i)
        {
            AVStream * inStream  = inputCtx->streams[i];
            AVStream * outStream = avformat_new_stream(outputCtx, nullptr);
            if (!outStream)
            {
                qDebug() << "分配 输出流失败。";
                avformat_close_input(&inputCtx);
                avformat_free_context(outputCtx);
                return;
            }
            avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
            outStream->codecpar->codec_tag = 0;
        }

        // 打开输出文件
        if (!(outputCtx->oformat->flags & AVFMT_NOFILE))
        {
            if (avio_open(&outputCtx->pb, outputFile.toStdString().c_str(), AVIO_FLAG_WRITE) < 0)
            {
                qDebug() << "打开输出文件失败。";
                avformat_close_input(&inputCtx);
                avformat_free_context(outputCtx);
                return;
            }
        }

        // 写入输出文件头
        if (avformat_write_header(outputCtx, nullptr) < 0)
        {
            qDebug() << "写入输出文件头失败";
            avio_closep(&outputCtx->pb);
            avformat_free_context(outputCtx);
            continue;
        }

        // 初始化段起始 PTS
        if (segmentStartPts == AV_NOPTS_VALUE)
        {
            if (inputCtx->streams[0]->start_time != AV_NOPTS_VALUE)
                segmentStartPts = inputCtx->streams[0]->start_time;
            else
                segmentStartPts = 0; // 默认设置为0
        }

        // 读取并写入指定帧数的流数据
        while (av_read_frame(inputCtx, &packet) >= 0 && framesCaptured < targetFrameCount)
        {
            AVStream * inStream  = inputCtx->streams[packet.stream_index];
            AVStream * outStream = outputCtx->streams[packet.stream_index];

            // 跳过无效时间戳的帧
            if (packet.pts == AV_NOPTS_VALUE || packet.dts == AV_NOPTS_VALUE)
            {
                av_packet_unref(&packet);
                continue;
            }

            // 将时间戳重置为相对值
            packet.pts      = av_rescale_q(packet.pts - segmentStartPts, inStream->time_base, outStream->time_base);
            packet.dts      = av_rescale_q(packet.dts - segmentStartPts, inStream->time_base, outStream->time_base);
            packet.duration = av_rescale_q(packet.duration, inStream->time_base, outStream->time_base);
            packet.pos      = -1;

            // 写入帧到输出文件
            if (av_interleaved_write_frame(outputCtx, &packet) < 0)
            {
                av_packet_unref(&packet);
                break;
            }

            av_packet_unref(&packet);
            framesCaptured++;
        }

        // 更新段起始 PTS
        if (framesCaptured > 0)
        {
            AVStream * firstStream = inputCtx->streams[0];
            segmentStartPts += targetFrameCount * firstStream->time_base.den / (fps * firstStream->time_base.num);
        }

        // 写入文件尾并清理
        av_write_trailer(outputCtx);

        if (!(outputCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&outputCtx->pb);
        avformat_free_context(outputCtx);
    }

    // 关闭输入流
    avformat_close_input(&inputCtx);
}

void VideoCaptureManager::manageDiskSpace(const QString & outputDir)
{
    QStorageInfo info      = QStorageInfo(outputDir);
    qint64       bytesFree = info.bytesFree();
    if (bytesFree < m_diskThresholdGB * 1024 * 1024 * 1024)
    {
        qDebug() << "磁盘空间不足，开始删除旧文件。";
        QDir          dir(outputDir);
        QFileInfoList files      = dir.entryInfoList(QStringList() << "*.mp4", QDir::Files, QDir::Time | QDir::Reversed);
        QFileInfo     oldestFile = files.takeFirst();
        QFile::remove(oldestFile.absoluteFilePath());
    }
}

qint64 VideoCaptureManager::getDirectorySize(const QString & path)
{
    qint64        size = 0;
    QDir          dir(path);
    QFileInfoList fileList = dir.entryInfoList(QDir::Files);
    for (const QFileInfo & fileInfo : fileList)
    {
        size += fileInfo.size();
    }
    return size;
}
