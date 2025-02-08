#include <QDebug>
#include <time.h>

extern "C"
{
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <SDL2/SDL.h>

#include "playerthread.h"

PlayerThread::PlayerThread(CircularQueue<AVFrame *> & frameQueue, QObject * parent)
    : QThread(parent)
    , m_frameQueue(frameQueue)
{
}

PlayerThread::~PlayerThread()
{
    stop();
}

void PlayerThread::stop()
{
    m_isOpened = false;
    requestInterruption();
    wait();

    m_frameQueue.stop();

    if (m_pCodecContext)
    {
        avcodec_close(m_pCodecContext);
        av_free(m_pCodecContext);
        m_pCodecContext = nullptr;
    }

    if (m_pFormatContext)
    {
        avformat_close_input(&m_pFormatContext);
        m_pFormatContext = nullptr;
    }
}

bool PlayerThread::openUrl(QString url, int & duration, int & fps)
{
    m_isOpened = false;
    stop();

    m_isOpened = openUrlRTSP(url, duration, fps);

    start();
    return m_isOpened;
}

void PlayerThread::seeToTime(double percent)
{
    m_frameQueue.clear();
    double seconds = (m_duration / 1000000.0) * (percent);
    m_seekTime.store(seconds * AV_TIME_BASE);
}

void PlayerThread::decode()
{
    AVPacket * packet = av_packet_alloc();
    AVFrame *  frame  = av_frame_alloc();

    while (!isInterruptionRequested())
    {
        int seektime = m_seekTime.load();
        // 处理跳转请求
        if (seektime >= 0 && seektime < m_duration)
        {
            int64_t targetPts = av_rescale_q(
                seektime, AVRational{1, AV_TIME_BASE}, m_pFormatContext->streams[m_videoStreamIndex]->time_base);

            if (av_seek_frame(m_pFormatContext, m_videoStreamIndex, targetPts, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY) < 0)
            {
                qDebug() << "跳转时间戳失败:" << targetPts;

                m_seekTime.store(-1);
                continue;
            }

            avcodec_flush_buffers(m_pCodecContext);
            m_seekTime.store(-1);
        }

        int readFrameResult = av_read_frame(m_pFormatContext, packet);
        if (readFrameResult >= 0)
        {
            if (packet->stream_index == m_videoStreamIndex)
            {
                if (avcodec_send_packet(m_pCodecContext, packet) < 0)
                {
                    qDebug() << "发送包到解码器失败。";
                    av_packet_unref(packet);
                    break;
                }

                while (true)
                {
                    int receiveResult = avcodec_receive_frame(m_pCodecContext, frame);
                    if (receiveResult == AVERROR(EAGAIN))
                    {
                        break;
                    }
                    else if (receiveResult == AVERROR_EOF)
                    {
                        qDebug() << "流结束(EOF)";
                        break;
                    }
                    else if (receiveResult < 0)
                    {
                        qDebug() << "帧解码失败:" << receiveResult;
                        break;
                    }

                    if (!m_frameQueue.isStopped())
                    {
                        // QImage img = convertFrameToQImage(frame, m_pCodecContext);
                        AVFrame * yuvFrame = av_frame_clone(frame);
                        m_frameQueue.enqueue(yuvFrame);
                    }
                }
            }
            av_packet_unref(packet);
        }
        else if (readFrameResult == AVERROR_EOF)
        {
            qDebug() << "文件结束。";
            break;
        }
        else if (readFrameResult == AVERROR(EIO) || readFrameResult == AVERROR(ENOMEM))
        {
            // 如果是 RTSP 连接错误或内存不足，尝试重连
            qDebug() << "RTSP 连接丢失或内存不足，尝试重新连接...";
            break;
        }
        else
        {
            qDebug() << "读取包失败:" << readFrameResult;
        }

        usleep(1000);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    emit videoStopped();
    m_isOpened = false;
}

void PlayerThread::run()
{
    if (m_isOpened)
    {
        decode();
    }
}

bool PlayerThread::openUrlRTSP(QString url, int & duration, int & fps)
{
    m_frameQueue.start();
    m_frameQueue.clear();
    av_log_set_level(AV_LOG_QUIET);
    m_filePath = url.toStdString();

    AVDictionary * options = nullptr;
    av_dict_set(&options, "stimeout", "10*1000*1000", 0); // 设置网络连接超时

    // 打开视频文件
    if (avformat_open_input(&m_pFormatContext, m_filePath.c_str(), nullptr, &options) < 0)
    {
        qDebug() << "无法打开视频文件，请检查路径是否正确。";
        av_dict_free(&options);
        return false;
    }
    av_dict_free(&options);
    // 查找流信息
    if (avformat_find_stream_info(m_pFormatContext, nullptr) < 0)
    {
        qDebug() << "无法找到视频流信息。";
        return false;
    }
    if (m_pFormatContext->duration != AV_NOPTS_VALUE)
    {
        m_duration = m_pFormatContext->duration; // 时长，单位是微秒（us）
        duration   = m_duration / 1000000;       // 时长，单位是秒
    }

    // 找到视频流
    m_videoStreamIndex = -1;
    for (unsigned int i = 0; i < m_pFormatContext->nb_streams; ++i)
    {
        if (m_pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_videoStreamIndex = i;
            break;
        }
    }

    if (m_videoStreamIndex == -1)
    {
        qDebug() << "未找到视频流。";
        return false;
    }
    // 获取视频流的帧率
    AVStream * pVideoStream = m_pFormatContext->streams[m_videoStreamIndex];
    AVRational frameRate    = pVideoStream->avg_frame_rate;

    fps = (int)(frameRate.num / (float)frameRate.den); // 帧率
    // 获取解码器
    AVCodecParameters * codecParams = m_pFormatContext->streams[m_videoStreamIndex]->codecpar;
    m_pCodec                        = avcodec_find_decoder(codecParams->codec_id);
    if (!m_pCodec)
    {
        qDebug() << "未找到解码器。";
        return false;
    }

    // 打开解码器
    m_pCodecContext = avcodec_alloc_context3(m_pCodec);
    if (avcodec_parameters_to_context(m_pCodecContext, codecParams) < 0)
    {
        qDebug() << "打开解码器失败。";
        return false;
    }

    if (avcodec_open2(m_pCodecContext, m_pCodec, nullptr) < 0)
    {
        qDebug() << "打开解码器失败。";
        return false;
    }

    return true;
}
