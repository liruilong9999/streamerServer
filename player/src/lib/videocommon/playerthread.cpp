/*!
 * \file  .\src\lib\videocommon\playerthread.cpp.
 *
 * Implements the playerthread class.
 */

#include <QDebug>
#include <QTimer>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include "playerthread.h"

namespace
{
int interruptReadCallback(void * opaque)
{
    auto * stopRequested = static_cast<std::atomic<bool> *>(opaque);
    return (stopRequested != nullptr && stopRequested->load()) ? 1 : 0;
}
}

PlayerThread::PlayerThread(CircularQueue<AVFrame *> & frameQueue, QObject * parent)
    : QThread(parent)
    , m_frameQueue(frameQueue)
{
    connect(this, &PlayerThread::reconnect, this, &PlayerThread::onReconnect, Qt::QueuedConnection);
}

PlayerThread::~PlayerThread()
{
    stop();
}

void PlayerThread::stop()
{
    m_frameQueue.stop();

    m_isOpened = false;
    m_stopRequested.store(true);
    requestInterruption();
    if (isRunning())
    {
        wait();
    }

    if (m_pCodecContext)
    {
        avcodec_free_context(&m_pCodecContext);
    }

    if (m_pFormatContext)
    {
        avformat_close_input(&m_pFormatContext);
        m_pFormatContext = nullptr;
    }
}

void PlayerThread::openUrl(QString url, int duration, int fps)
{
    m_isOpened = false;
    stop();

    m_url      = url;
    m_fps      = fps;
    m_duration = duration;
    m_stopRequested.store(false);

    start();
}

void PlayerThread::decode()
{
    AVPacket * packet = av_packet_alloc();
    AVFrame *  frame  = av_frame_alloc();
    if (packet == nullptr || frame == nullptr)
    {
        if (frame)
        {
            av_frame_free(&frame);
        }
        if (packet)
        {
            av_packet_free(&packet);
        }
        m_isOpened = false;
        emit videoStopped();
        return;
    }

    while (!isInterruptionRequested() && !m_stopRequested.load())
    {
        const int readFrameResult = av_read_frame(m_pFormatContext, packet);
        if (isInterruptionRequested() || m_stopRequested.load() || readFrameResult == AVERROR_EXIT)
        {
            av_packet_unref(packet);
            break;
        }

        if (readFrameResult >= 0)
        {
            if (packet->stream_index == m_videoStreamIndex && m_pCodecContext != nullptr)
            {
                if (avcodec_send_packet(m_pCodecContext, packet) < 0)
                {
                    av_packet_unref(packet);
                    break;
                }

                while (true)
                {
                    const int receiveResult = avcodec_receive_frame(m_pCodecContext, frame);
                    if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF)
                    {
                        break;
                    }
                    if (receiveResult < 0)
                    {
                        break;
                    }

                    if (!m_frameQueue.isStopped())
                    {
                        AVFrame * yuvFrame = av_frame_clone(frame);
                        if (yuvFrame != nullptr)
                        {
                            m_frameQueue.enqueue(yuvFrame);
                        }
                    }
                }
            }
            av_packet_unref(packet);
        }
        else if (readFrameResult == AVERROR_EOF)
        {
            if (!m_stopRequested.load())
            {
                qDebug() << QString("流结束，准备重连");
                emit reconnect();
            }
            break;
        }
        else if (readFrameResult == AVERROR(EIO) || readFrameResult == AVERROR(ENOMEM))
        {
            if (!m_stopRequested.load())
            {
                qDebug() << QString("流读取异常，准备重连");
                emit reconnect();
            }
            break;
        }
        else
        {
            if (!m_stopRequested.load())
            {
                qDebug() << QString("读取数据包失败:") << readFrameResult;
                emit reconnect();
            }
            break;
        }

        QThread::msleep(1);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    m_isOpened = false;
    emit videoStopped();
}

void PlayerThread::onReconnect()
{
    if (m_stopRequested.load())
    {
        return;
    }

    QTimer::singleShot(1000, this, [&]() {
        if (!m_stopRequested.load())
        {
            openUrl(m_url, m_duration, m_fps);
        }
    });
}

void PlayerThread::run()
{
    m_isOpened = openUrlRTSP(m_url, m_duration, m_fps);
    if (m_isOpened)
    {
        emit openSuccess(m_url, m_duration, m_fps);
        decode();
    }
    else
    {
        emit videoStopped();
        if (!m_stopRequested.load())
        {
            QTimer::singleShot(5000, this, [&]() {
                if (!m_stopRequested.load())
                {
                    openUrl(m_url, m_duration, m_fps);
                }
            });
        }
    }
}

bool PlayerThread::openUrlRTSP(QString url, int & duration, int & fps)
{
    m_frameQueue.start();
    m_frameQueue.clear();

    av_log_set_level(AV_LOG_QUIET);
    m_filePath = url.toStdString();

    m_pFormatContext = avformat_alloc_context();
    if (m_pFormatContext == nullptr)
    {
        return false;
    }
    m_pFormatContext->interrupt_callback.callback = interruptReadCallback;
    m_pFormatContext->interrupt_callback.opaque   = &m_stopRequested;

    AVDictionary * options = nullptr;
    av_dict_set(&options, "timeout", "3000000", 0);
    av_dict_set(&options, "rtsp_transport", "udp", 0);
    av_dict_set(&options, "muxdelay", "0.1", 0);
    av_dict_set(&options, "buffer_size", "1024000", 0);

    if (avformat_open_input(&m_pFormatContext, m_filePath.c_str(), nullptr, &options) < 0)
    {
        av_dict_free(&options);
        avformat_close_input(&m_pFormatContext);
        qDebug() << QString("打开输入失败:") << url;
        return false;
    }
    av_dict_free(&options);

    if (avformat_find_stream_info(m_pFormatContext, nullptr) < 0)
    {
        avformat_close_input(&m_pFormatContext);
        qDebug() << QString("查找流信息失败");
        return false;
    }

    if (m_pFormatContext->duration != AV_NOPTS_VALUE)
    {
        m_duration = static_cast<int>(m_pFormatContext->duration);
        duration   = m_duration / 1000000;
    }

    m_videoStreamIndex = -1;
    for (unsigned int i = 0; i < m_pFormatContext->nb_streams; ++i)
    {
        const AVMediaType type = m_pFormatContext->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO)
        {
            m_videoStreamIndex = static_cast<int>(i);
            break;
        }
    }

    if (m_videoStreamIndex == -1)
    {
        avformat_close_input(&m_pFormatContext);
        qDebug() << QString("未找到视频流");
        return false;
    }

    AVStream * pVideoStream = m_pFormatContext->streams[m_videoStreamIndex];
    AVRational frameRate    = pVideoStream->avg_frame_rate;
    if (frameRate.den != 0)
    {
        fps = static_cast<int>(frameRate.num / static_cast<float>(frameRate.den));
    }

    AVCodecParameters * codecParams = pVideoStream->codecpar;
    m_pCodec                        = const_cast<AVCodec *>(avcodec_find_decoder(codecParams->codec_id));
    if (m_pCodec == nullptr)
    {
        avformat_close_input(&m_pFormatContext);
        qDebug() << QString("查找解码器失败");
        return false;
    }

    m_pCodecContext = avcodec_alloc_context3(m_pCodec);
    if (m_pCodecContext == nullptr)
    {
        avformat_close_input(&m_pFormatContext);
        qDebug() << QString("分配解码器上下文失败");
        return false;
    }

    if (avcodec_parameters_to_context(m_pCodecContext, codecParams) < 0)
    {
        avcodec_free_context(&m_pCodecContext);
        avformat_close_input(&m_pFormatContext);
        qDebug() << QString("拷贝解码参数失败");
        return false;
    }

    if (avcodec_open2(m_pCodecContext, m_pCodec, nullptr) < 0)
    {
        avcodec_free_context(&m_pCodecContext);
        avformat_close_input(&m_pFormatContext);
        qDebug() << QString("打开解码器失败");
        return false;
    }

    return true;
}
