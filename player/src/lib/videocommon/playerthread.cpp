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
#include <QTimer>

#include "playerthread.h"

PlayerThread::PlayerThread(CircularQueue<AVFrame *> & frameQueue, CircularQueue<QString> & strQueue, QObject * parent)
    : QThread(parent)
    , m_frameQueue(frameQueue)
    , m_StrQueue(strQueue)
{
    connect(this, &PlayerThread::reconnect, this, &PlayerThread::onReconnect, Qt::QueuedConnection);
}

PlayerThread::~PlayerThread()
{
    stop();
}

void PlayerThread::releaseFfmpegContexts()
{
    if (nullptr != m_pSubtitleCodecContext)
    {
        avcodec_free_context(&m_pSubtitleCodecContext);
    }

    if (nullptr != m_pCodecContext)
    {
        avcodec_free_context(&m_pCodecContext);
    }

    if (nullptr != m_pFormatContext)
    {
        avformat_close_input(&m_pFormatContext);
    }

    m_pCodec              = nullptr;
    m_videoStreamIndex    = -1;
    m_subtitleStreamIndex = -1;
}

void PlayerThread::clearFrameQueueAndReleaseFrames()
{
    const bool wasStopped = m_frameQueue.isStopped();
    m_frameQueue.stop();

    AVFrame * queuedFrame = nullptr;
    while (true)
    {
        if (false == m_frameQueue.dequeue(queuedFrame))
        {
            break;
        }

        if (nullptr != queuedFrame)
        {
            av_frame_free(&queuedFrame);
        }
    }

    if (false == wasStopped)
    {
        m_frameQueue.start();
    }
}

void PlayerThread::stop()
{
    m_isOpened = false;
    requestInterruption();
    wait();

    m_frameQueue.stop();
    m_StrQueue.stop();

    clearFrameQueueAndReleaseFrames();
    m_StrQueue.clear();
    releaseFfmpegContexts();
}

void PlayerThread::openUrl(QString url, int duration, int fps)
{
    m_isOpened = false;
    stop();

    m_url      = url;
    m_fps      = fps;
    m_duration = duration;

    start();
}

void PlayerThread::seeToTime(double percent)
{
    clearFrameQueueAndReleaseFrames();
    m_StrQueue.clear();

    const double seconds = (m_duration / 1000000.0) * percent;
    m_seekTime.store(seconds * AV_TIME_BASE);
}

void PlayerThread::decode()
{
    AVPacket * packet = av_packet_alloc();
    AVFrame *  frame  = av_frame_alloc();
    AVSubtitle subtitle{};

    if (nullptr == packet || nullptr == frame)
    {
        qDebug() << "分配 AVPacket 或 AVFrame 失败。";
        av_frame_free(&frame);
        av_packet_free(&packet);
        emit videoStopped();
        m_isOpened = false;
        return;
    }

    while (false == isInterruptionRequested())
    {
        const int seektime = m_seekTime.load();
        if (seektime >= 0 && seektime < m_duration)
        {
            const int64_t targetPts = av_rescale_q(
                seektime, AVRational{1, AV_TIME_BASE}, m_pFormatContext->streams[m_videoStreamIndex]->time_base);

            if (0 > av_seek_frame(m_pFormatContext, m_videoStreamIndex, targetPts, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY))
            {
                qDebug() << "跳转时间戳失败:" << targetPts;
                m_seekTime.store(-1);
                continue;
            }

            avcodec_flush_buffers(m_pCodecContext);
            if (nullptr != m_pSubtitleCodecContext)
            {
                avcodec_flush_buffers(m_pSubtitleCodecContext);
            }

            m_seekTime.store(-1);
        }

        const int readFrameResult = av_read_frame(m_pFormatContext, packet);
        if (0 <= readFrameResult)
        {
            if (m_videoStreamIndex == packet->stream_index)
            {
                if (0 > avcodec_send_packet(m_pCodecContext, packet))
                {
                    qDebug() << "发送包到解码器失败。";
                    av_packet_unref(packet);
                    break;
                }

                while (true)
                {
                    const int receiveResult = avcodec_receive_frame(m_pCodecContext, frame);
                    if (AVERROR(EAGAIN) == receiveResult)
                    {
                        break;
                    }

                    if (AVERROR_EOF == receiveResult)
                    {
                        qDebug() << "流结束(EOF)";
                        break;
                    }

                    if (0 > receiveResult)
                    {
                        qDebug() << "帧解码失败:" << receiveResult;
                        break;
                    }

                    if (false == m_frameQueue.isStopped())
                    {
                        AVFrame * yuvFrame = av_frame_clone(frame);
                        if (nullptr == yuvFrame)
                        {
                            qDebug() << "克隆视频帧失败。";
                            continue;
                        }

                        if (false == m_frameQueue.enqueue(yuvFrame))
                        {
                            av_frame_free(&yuvFrame);
                        }
                    }
                }
            }
            else if (m_subtitleStreamIndex == packet->stream_index && nullptr != m_pSubtitleCodecContext)
            {
                int gotSubtitle = 0;
                const int ret = avcodec_decode_subtitle2(m_pSubtitleCodecContext, &subtitle, &gotSubtitle, packet);
                if (0 > ret)
                {
                    qDebug() << "字幕解码失败";
                }
                else if (0 != gotSubtitle)
                {
                    for (uint32_t i = 0; i < subtitle.num_rects; ++i)
                    {
                        AVSubtitleRect * rect = subtitle.rects[i];
                        if (nullptr != rect->text)
                        {
                            if (false == m_StrQueue.isStopped())
                            {
                                m_StrQueue.enqueue(rect->text);
                            }
                        }
                        else if (nullptr != rect->ass)
                        {
                            if (false == m_StrQueue.isStopped())
                            {
                                m_StrQueue.enqueue(rect->ass);
                            }
                        }
                    }
                    avsubtitle_free(&subtitle);
                }
            }

            av_packet_unref(packet);
        }
        else if (AVERROR_EOF == readFrameResult)
        {
            qDebug() << "文件结束。";
            break;
        }
        else if (AVERROR(EIO) == readFrameResult || AVERROR(ENOMEM) == readFrameResult)
        {
            qDebug() << "RTSP 连接丢失或内存不足，尝试重新连接...";
            emit reconnect();
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

void PlayerThread::onReconnect()
{
    QTimer::singleShot(1000, this, [&]() {
        openUrl(m_url, m_duration, m_fps);
    });
}

void PlayerThread::run()
{
    m_isOpened = openUrlRTSP(m_url, m_duration, m_fps);
    if (true == m_isOpened)
    {
        emit openSuccess(m_url, m_duration, m_fps);
        decode();
    }
    else
    {
        QTimer::singleShot(5000, this, [&]() {
            openUrl(m_url, m_duration, m_fps);
        });
    }
}

bool PlayerThread::openUrlRTSP(QString url, int & duration, int & fps)
{
    releaseFfmpegContexts();

    m_frameQueue.start();
    clearFrameQueueAndReleaseFrames();
    m_StrQueue.start();
    m_StrQueue.clear();

    av_log_set_level(AV_LOG_QUIET);
    m_filePath = url.toStdString();

    AVDictionary * options = nullptr;
    av_dict_set(&options, "stimeout", "5*1000*1000", 0);

    if (0 > avformat_open_input(&m_pFormatContext, m_filePath.c_str(), nullptr, &options))
    {
        qDebug() << "无法打开视频文件，请检查路径是否正确。";
        av_dict_free(&options);
        releaseFfmpegContexts();
        return false;
    }
    av_dict_free(&options);

    if (0 > avformat_find_stream_info(m_pFormatContext, nullptr))
    {
        qDebug() << "无法找到视频流信息。";
        releaseFfmpegContexts();
        return false;
    }

    if (AV_NOPTS_VALUE != m_pFormatContext->duration)
    {
        m_duration = m_pFormatContext->duration;
        duration   = m_duration / 1000000;
    }

    m_videoStreamIndex    = -1;
    m_subtitleStreamIndex = -1;

    for (unsigned int i = 0; i < m_pFormatContext->nb_streams; ++i)
    {
        const AVMediaType type = m_pFormatContext->streams[i]->codecpar->codec_type;
        if (AVMEDIA_TYPE_VIDEO == type && -1 == m_videoStreamIndex)
        {
            m_videoStreamIndex = static_cast<int>(i);
        }
        else if (AVMEDIA_TYPE_SUBTITLE == type && -1 == m_subtitleStreamIndex)
        {
            m_subtitleStreamIndex = static_cast<int>(i);
        }
    }

    if (-1 == m_videoStreamIndex)
    {
        qDebug() << "未找到视频流。";
        releaseFfmpegContexts();
        return false;
    }

    AVStream *  pVideoStream = m_pFormatContext->streams[m_videoStreamIndex];
    AVRational  frameRate    = pVideoStream->avg_frame_rate;
    if (0 == frameRate.den || 0 == frameRate.num)
    {
        fps = 25;
        qDebug() << "输入流帧率无效，回退为 25。";
    }
    else
    {
        fps = static_cast<int>(frameRate.num / static_cast<float>(frameRate.den));
    }

    AVCodecParameters * codecParams = m_pFormatContext->streams[m_videoStreamIndex]->codecpar;
    m_pCodec                        = const_cast<AVCodec *>(avcodec_find_decoder(codecParams->codec_id));
    if (nullptr == m_pCodec)
    {
        qDebug() << "未找到视频解码器。";
        releaseFfmpegContexts();
        return false;
    }

    m_pCodecContext = avcodec_alloc_context3(m_pCodec);
    if (nullptr == m_pCodecContext)
    {
        qDebug() << "分配视频解码器上下文失败。";
        releaseFfmpegContexts();
        return false;
    }

    if (0 > avcodec_parameters_to_context(m_pCodecContext, codecParams))
    {
        qDebug() << "视频解码器参数设置失败。";
        releaseFfmpegContexts();
        return false;
    }

    if (0 > avcodec_open2(m_pCodecContext, m_pCodec, nullptr))
    {
        qDebug() << "打开视频解码器失败。";
        releaseFfmpegContexts();
        return false;
    }

    if (-1 != m_subtitleStreamIndex)
    {
        AVCodecParameters * subParams = m_pFormatContext->streams[m_subtitleStreamIndex]->codecpar;
        AVCodec *           subCodec  = const_cast<AVCodec *>(avcodec_find_decoder(subParams->codec_id));
        if (nullptr == subCodec)
        {
            qDebug() << "未找到字幕解码器。";
        }
        else
        {
            m_pSubtitleCodecContext = avcodec_alloc_context3(subCodec);
            if (nullptr == m_pSubtitleCodecContext)
            {
                qDebug() << "分配字幕解码器上下文失败。";
            }
            else if (0 > avcodec_parameters_to_context(m_pSubtitleCodecContext, subParams))
            {
                qDebug() << "字幕解码器参数设置失败。";
                avcodec_free_context(&m_pSubtitleCodecContext);
            }
            else if (0 > avcodec_open2(m_pSubtitleCodecContext, subCodec, nullptr))
            {
                qDebug() << "打开字幕解码器失败。";
                avcodec_free_context(&m_pSubtitleCodecContext);
            }
            else
            {
                qDebug() << "字幕流已打开 (index=" << m_subtitleStreamIndex << ")";
            }
        }
    }
    else
    {
        qDebug() << "未找到字幕流。";
    }

    return true;
}
