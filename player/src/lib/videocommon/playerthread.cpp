/*!
 * \file  .\src\lib\videocommon\playerthread.cpp.
 *
 * Implements the playerthread class
 */

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

/*!
 * 初始化<see cref="PlayerThread"/>类的新实例
 *
 * \author bf
 * \date 2025/8/5
 *
 * \param [in,out] frameQueue [in,out] 若非空，帧队列.
 * \param [in,out] strQueue 队列。
 * \param [in,out] parent 如果非空，则为父对象。
 */

PlayerThread::PlayerThread(CircularQueue<AVFrame *> & frameQueue, CircularQueue<QString> & strQueue, QObject * parent)
    : QThread(parent)
    , m_frameQueue(frameQueue)
   // , m_StrQueue(strQueue)
{
    connect(this, &PlayerThread::reconnect, this, &PlayerThread::onReconnect, Qt::QueuedConnection);
}

/*!
 * 析构<see cref="PlayerThread"/>类的实例
 *
 * \author bf
 * \date 2025/8/5
 */

PlayerThread::~PlayerThread()
{
    stop();
}

/*!
 * 停止此对象
 *
 * \author bf
 * \date 2025/8/5
 */

void PlayerThread::stop()
{
    m_frameQueue.stop();
   // m_StrQueue.stop();

    m_isOpened = false;
    requestInterruption();
    wait();

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

/*!
 * 打开URL。
 *
 * \author bf
 * \date 2025/8/5
 *
 * \param  文档URL.
 * \param  duration 持续时间
 * \param  fps 帧率。
 */

void PlayerThread::openUrl(QString url, int duration, int fps)
{
    m_isOpened = false;
    stop();

    m_url      = url;
    m_fps      = fps;
    m_duration = duration;

    start();
}

/*!
 * 处理时间。
 *
 * \author bf
 * \date 2025/8/5
 *
 * \param  百分比.
 */

void PlayerThread::seeToTime(double percent)
{
    m_frameQueue.clear();
   // m_StrQueue.clear();
    double seconds = (m_duration / 1000000.0) * (percent);
    m_seekTime.store(seconds * AV_TIME_BASE);
}

/*!
 * 解码此对象。
 *
 * \author bf
 * \date 2025/8/5
 */

void PlayerThread::decode()
{
    AVPacket * packet = av_packet_alloc();
    AVFrame *  frame  = av_frame_alloc();

    while (!isInterruptionRequested())
    {
        int seektime = m_seekTime.load();
        // 处理跳转请求
        if ((seektime >= 0) && (seektime < m_duration))
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
//            if (m_pSubtitleCodecContext)
//            {avcodec_flush_buffers(m_pSubtitleCodecContext);}

            m_seekTime.store(-1);
        }

        int readFrameResult = av_read_frame(m_pFormatContext, packet);
        if (readFrameResult >= 0)
        {
            // 🎥 视频流
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
                          { break;}
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
            emit reconnect();
            break;
        }
        else if (readFrameResult == AVERROR(EIO) || readFrameResult == AVERROR(ENOMEM))
        {
            qDebug() << "RTSP 连接丢失或内存不足，尝试重新连接...";
            emit reconnect();
            break;
        }
        else
        {
            qDebug() << "读取包失败:" << readFrameResult;
            emit reconnect();
            break;
        }

        usleep(1000);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    m_isOpened = false;
    emit videoStopped();
}

/*!
 * 执行 '重新连接' 动作
 *
 * \author bf
 * \date 2025/8/5
 */

void PlayerThread::onReconnect()
{
    QTimer::singleShot(1000, this, [&]() {
        openUrl(m_url, m_duration, m_fps);
    });
}

/*!
 * 运行此对象
 *
 * \author bf
 * \date 2025/8/5
 */

void PlayerThread::run()
{
    m_isOpened = openUrlRTSP(m_url, m_duration, m_fps);
    if (m_isOpened)
    {
        emit openSuccess(m_url, m_duration, m_fps);
        decode();
    }
//    else
//    {
//        QTimer::singleShot(5000, this, [&]() {
//            openUrl(m_url, m_duration, m_fps);
//        });
//    }
}

/*!
 * 打开RTSP URL。
 *
 * \author bf
 * \date 2025/8/5
 *
 * \param 		   文档URL.
 * \param 持续时间。
 * \param [输入/输出] fps FPS.
 *
 * \returns 如果成功则为真，否则为假。
 */

bool PlayerThread::openUrlRTSP(QString url, int & duration, int & fps)
{
    m_frameQueue.start();
    m_frameQueue.clear();

//    m_StrQueue.start();
//    m_StrQueue.clear();

    av_log_set_level(AV_LOG_QUIET);
    m_filePath = url.toStdString();

    AVDictionary * options = nullptr;
    av_dict_set(&options, "timeout", "3000000", 0);     // 设置网络连接超时
    av_dict_set(&options, "rtsp_transport", "udp", 0);  //
    av_dict_set(&options, "muxdelay", "0.1", 0);        // 输出延时
    av_dict_set(&options, "buffer_size", "1024000", 0); // 缓存大小

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
    m_videoStreamIndex    = -1;
//    m_subtitleStreamIndex = -1;

    for (unsigned int i = 0; i < m_pFormatContext->nb_streams; ++i)
    {
        AVMediaType type = m_pFormatContext->streams[i]->codecpar->codec_type;
        if ((type == AVMEDIA_TYPE_VIDEO) && (m_videoStreamIndex == -1))
        {
            m_videoStreamIndex = i;
        }
//        else if ((type == AVMEDIA_TYPE_SUBTITLE) && (m_subtitleStreamIndex == -1))
//        {
//            m_subtitleStreamIndex = i;
//        }
    }

    if (m_videoStreamIndex == -1)
    {
        qDebug() << "未找到视频流。";
        return false;
    }

    // 获取视频流的帧率
    AVStream * pVideoStream = m_pFormatContext->streams[m_videoStreamIndex];
    AVRational frameRate    = pVideoStream->avg_frame_rate;
    fps                     = (int)(frameRate.num / (float)frameRate.den); // 帧率

    // 打开视频解码器
    AVCodecParameters * codecParams = m_pFormatContext->streams[m_videoStreamIndex]->codecpar;
    m_pCodec                        = const_cast<AVCodec *>(avcodec_find_decoder(codecParams->codec_id));
    if (nullptr == m_pCodec)
    {
        qDebug() << "未找到视频解码器。";
        return false;
    }

    m_pCodecContext = avcodec_alloc_context3(m_pCodec);
    if (avcodec_parameters_to_context(m_pCodecContext, codecParams) < 0)
    {
        qDebug() << "视频解码器参数设置失败。";
        return false;
    }

    if (avcodec_open2(m_pCodecContext, m_pCodec, nullptr) < 0)
    {
        qDebug() << "打开视频解码器失败。";
        return false;
    }

//    // 打开字幕解码器（如果有的话）
//    if (m_subtitleStreamIndex != -1)
//    {
//        AVCodecParameters * subParams = m_pFormatContext->streams[m_subtitleStreamIndex]->codecpar;
//        AVCodec *           subCodec  = const_cast<AVCodec *>(avcodec_find_decoder(subParams->codec_id));
//        if (nullptr == subCodec)
//        {
//            qDebug() << "未找到字幕解码器。";
//        }
//        else
//        {
//            m_pSubtitleCodecContext = avcodec_alloc_context3(subCodec);
//            if (avcodec_parameters_to_context(m_pSubtitleCodecContext, subParams) < 0)
//            {
//                qDebug() << "字幕解码器参数设置失败。";
//            }
//            else if (avcodec_open2(m_pSubtitleCodecContext, subCodec, nullptr) < 0)
//            {
//                qDebug() << "打开字幕解码器失败。";
//            }
//            else
//            {
//                qDebug() << "字幕流已打开 (index=" << m_subtitleStreamIndex << ")";
//            }
//        }
//    }
//    else
//    {
//        qDebug() << "未找到字幕流。";
//    }

    return true;
}
