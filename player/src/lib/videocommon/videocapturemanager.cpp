#include "videocapturemanager.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QMutexLocker>
#include <QStorageInfo>

#include <cmath>
#include <chrono>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
}

namespace
{
constexpr int64_t kMicrosecondsPerSecond = 1000000LL;

/**
 * @brief FFmpeg 读流中断回调。
 * @param opaque 指向 std::atomic<bool> 的指针。
 * @return 1 表示中断；0 表示继续。
 */
int interruptReadCallback(void * opaque)
{
    auto * running = static_cast<std::atomic<bool> *>(opaque);
    return (running != nullptr && !running->load()) ? 1 : 0;
}

/**
 * @brief 获取包时间戳（优先 pts，缺失时回退 dts）。
 */
int64_t packetTimestamp(const AVPacket * packet)
{
    if (packet == nullptr)
    {
        return AV_NOPTS_VALUE;
    }

    if (packet->pts != AV_NOPTS_VALUE)
    {
        return packet->pts;
    }

    return packet->dts;
}

/**
 * @brief FFmpeg 错误码转可读文本。
 */
QString ffmpegErrorString(int errorCode)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errorCode, errorBuffer, sizeof(errorBuffer));
    return QString::fromLocal8Bit(errorBuffer);
}

/**
 * @brief 释放输出上下文（不写 trailer）。
 */
void freeOutputContext(AVFormatContext ** outputCtx)
{
    if (outputCtx == nullptr || *outputCtx == nullptr)
    {
        return;
    }

    if (!((*outputCtx)->oformat->flags & AVFMT_NOFILE))
    {
        avio_closep(&((*outputCtx)->pb));
    }

    avformat_free_context(*outputCtx);
    *outputCtx = nullptr;
}

/**
 * @brief 关闭输出上下文（写 trailer + 释放）。
 */
void closeOutputContext(AVFormatContext ** outputCtx)
{
    if (outputCtx == nullptr || *outputCtx == nullptr)
    {
        return;
    }

    av_write_trailer(*outputCtx);
    freeOutputContext(outputCtx);
}

/**
 * @brief 仅为视频流创建一个输出 MP4 上下文。
 * @param inputCtx 输入上下文。
 * @param videoStreamIndex 输入视频流索引。
 * @param outputFile 输出文件路径。
 * @param outputCtx 输出上下文。
 * @param errorCode 错误码。
 * @return true 成功。
 * @return false 失败。
 */
bool createVideoOnlyOutputContext(AVFormatContext * inputCtx,
                                  int               videoStreamIndex,
                                  const QString &   outputFile,
                                  AVFormatContext ** outputCtx,
                                  int &              errorCode)
{
    errorCode = 0;

    if (inputCtx == nullptr || outputCtx == nullptr || videoStreamIndex < 0 || videoStreamIndex >= static_cast<int>(inputCtx->nb_streams))
    {
        errorCode = AVERROR(EINVAL);
        return false;
    }

    *outputCtx = nullptr;

    const QByteArray outputFileUtf8 = outputFile.toUtf8();
    errorCode = avformat_alloc_output_context2(outputCtx, nullptr, nullptr, outputFileUtf8.constData());
    if (errorCode < 0 || *outputCtx == nullptr)
    {
        if (errorCode >= 0)
        {
            errorCode = AVERROR_UNKNOWN;
        }
        return false;
    }

    AVStream * inStream  = inputCtx->streams[videoStreamIndex];
    AVStream * outStream = avformat_new_stream(*outputCtx, nullptr);
    if (outStream == nullptr)
    {
        errorCode = AVERROR(ENOMEM);
        freeOutputContext(outputCtx);
        return false;
    }

    errorCode = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
    if (errorCode < 0)
    {
        freeOutputContext(outputCtx);
        return false;
    }

    outStream->codecpar->codec_tag = 0;
    outStream->time_base           = inStream->time_base;
    outStream->avg_frame_rate      = inStream->avg_frame_rate;
    outStream->r_frame_rate        = inStream->r_frame_rate;

    if (!((*outputCtx)->oformat->flags & AVFMT_NOFILE))
    {
        errorCode = avio_open(&((*outputCtx)->pb), outputFileUtf8.constData(), AVIO_FLAG_WRITE);
        if (errorCode < 0)
        {
            avformat_free_context(*outputCtx);
            *outputCtx = nullptr;
            return false;
        }
    }

    errorCode = avformat_write_header(*outputCtx, nullptr);
    if (errorCode < 0)
    {
        if (!((*outputCtx)->oformat->flags & AVFMT_NOFILE))
        {
            avio_closep(&((*outputCtx)->pb));
        }
        avformat_free_context(*outputCtx);
        *outputCtx = nullptr;
        return false;
    }

    return true;
}
} // namespace

VideoCaptureManager::VideoCaptureManager(QObject * parent)
    : QThread(parent)
    , m_isRunning(false)
{}

VideoCaptureManager::~VideoCaptureManager()
{
    stop();
}

bool VideoCaptureManager::start(const QString & rtspUrl,
                                const QString & saveDir,
                                unsigned        segmentDurationSec,
                                unsigned        diskThresholdGB,
                                unsigned        targetDurationSec)
{
    if (m_isRunning.load() || QThread::isRunning())
    {
        qDebug() << "采集任务已在运行，不能重复启动。";
        return false;
    }

    QString errorMessage;
    if (!validateStartParams(rtspUrl, saveDir, segmentDurationSec, diskThresholdGB, targetDurationSec, errorMessage))
    {
        qDebug() << "启动失败，参数不合法:" << errorMessage;
        return false;
    }

    const QString normalizedSaveDir = saveDir.trimmed();
    if (!prepareSaveDir(normalizedSaveDir))
    {
        qDebug() << "启动失败，创建保存目录失败:" << normalizedSaveDir;
        return false;
    }

    {
        QMutexLocker locker(&m_stateMutex);
        m_inputUrl        = rtspUrl.trimmed();
        m_outputDir       = normalizedSaveDir;
        m_segmentDuration = segmentDurationSec;
        m_diskThresholdGB = diskThresholdGB;
        m_targetDuration  = targetDurationSec;
    }

    m_isRunning.store(true);
    QThread::start();

    qDebug() << "采集线程已启动。RTSP:" << rtspUrl << "保存目录:" << normalizedSaveDir
             << "分段时长(秒):" << segmentDurationSec << "磁盘阈值(GB):" << diskThresholdGB
             << "目标总时长(秒):" << targetDurationSec;
    return true;
}

void VideoCaptureManager::stop()
{
    const bool wasRunning = m_isRunning.exchange(false);
    if (!wasRunning && !QThread::isRunning())
    {
        return;
    }

    wait();
    qDebug() << "采集线程已停止。";
}

void VideoCaptureManager::startCapture(const QString & inputUrl)
{
    (void)start(inputUrl, QStringLiteral("video"), 3600, 10, 0);
}

void VideoCaptureManager::stopCapture()
{
    stop();
}

void VideoCaptureManager::run()
{
    using SteadyClock = std::chrono::steady_clock;
    using namespace std::chrono;

    QString  inputUrl;
    QString  outputDir;
    unsigned segmentDurationSec = 3600;
    unsigned targetDurationSec  = 0;

    {
        QMutexLocker locker(&m_stateMutex);
        inputUrl           = m_inputUrl;
        outputDir          = m_outputDir;
        segmentDurationSec = m_segmentDuration;
        targetDurationSec  = m_targetDuration;
    }

    AVFormatContext * inputCtx     = nullptr;
    AVFormatContext * outputCtx = nullptr;
    int               videoStreamIndex = -1;
    int               segmentIndex     = 0;

    bool    waitingKeyFrame = true;
    bool    pendingRotate   = false;
    int64_t segmentStartPts = AV_NOPTS_VALUE;
    int64_t segmentStartDts = AV_NOPTS_VALUE;

    const int64_t segmentDurationUs = static_cast<int64_t>(segmentDurationSec) * kMicrosecondsPerSecond;
    const int64_t targetDurationUs  = (targetDurationSec == 0) ? 0 : static_cast<int64_t>(targetDurationSec) * kMicrosecondsPerSecond;

    const SteadyClock::time_point recordStartWall = SteadyClock::now();
    SteadyClock::time_point       segmentStartWall = recordStartWall;

    AVPacket * packet = av_packet_alloc();
    if (packet == nullptr)
    {
        qDebug() << "采集线程初始化失败：无法分配 AVPacket。";
        m_isRunning.store(false);
        return;
    }

    auto resetSegmentState = [&]() {
        waitingKeyFrame   = true;
        pendingRotate     = false;
        segmentStartPts   = AV_NOPTS_VALUE;
        segmentStartDts   = AV_NOPTS_VALUE;
        segmentStartWall  = SteadyClock::now();
    };

    auto closeInput = [&]() {
        if (inputCtx != nullptr)
        {
            avformat_close_input(&inputCtx);
            inputCtx = nullptr;
        }
        videoStreamIndex = -1;
    };

    auto openInput = [&]() -> bool {
        inputCtx = avformat_alloc_context();
        if (inputCtx == nullptr)
        {
            qDebug() << "分配输入上下文失败。";
            return false;
        }

        inputCtx->interrupt_callback.callback = interruptReadCallback;
        inputCtx->interrupt_callback.opaque   = &m_isRunning;

        AVDictionary * options = nullptr;
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
        av_dict_set(&options, "stimeout", "5000000", 0);
        av_dict_set(&options, "rw_timeout", "5000000", 0);
        av_dict_set(&options, "max_delay", "500000", 0);
        av_dict_set(&options, "buffer_size", "1024000", 0);
        av_dict_set(&options, "use_wallclock_as_timestamps", "1", 0);
        av_dict_set(&options, "fflags", "+genpts", 0);

        const QByteArray inputUrlUtf8 = inputUrl.toUtf8();
        int              retOpen      = avformat_open_input(&inputCtx, inputUrlUtf8.constData(), nullptr, &options);
        av_dict_free(&options);

        if (retOpen < 0)
        {
            qDebug() << "打开 RTSP 输入失败:" << inputUrl << "错误:" << ffmpegErrorString(retOpen);
            if (inputCtx != nullptr)
            {
                avformat_free_context(inputCtx);
                inputCtx = nullptr;
            }
            return false;
        }

        retOpen = avformat_find_stream_info(inputCtx, nullptr);
        if (retOpen < 0)
        {
            qDebug() << "读取流信息失败，错误:" << ffmpegErrorString(retOpen);
            closeInput();
            return false;
        }

        videoStreamIndex = av_find_best_stream(inputCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (videoStreamIndex < 0)
        {
            qDebug() << "未找到视频流，等待重连。";
            closeInput();
            return false;
        }

        m_fps = detectFps(inputCtx, videoStreamIndex);
        if (m_fps <= 0)
        {
            m_fps = 25;
            qDebug() << "未检测到有效帧率，使用默认帧率 25 fps。";
        }

        qDebug() << "RTSP 已连接。视频流索引:" << videoStreamIndex << "检测帧率:" << m_fps;
        return true;
    };

    while (m_isRunning.load())
    {
        const int64_t wallElapsedUs = duration_cast<microseconds>(SteadyClock::now() - recordStartWall).count();
        if (targetDurationUs > 0 && wallElapsedUs >= targetDurationUs)
        {
            qDebug() << "已达到目标本机时长(秒):" << targetDurationSec << "结束录制。";
            break;
        }

        if (inputCtx == nullptr)
        {
            if (!openInput())
            {
                if (!m_isRunning.load())
                {
                    break;
                }

                qDebug() << "连接失败，1 秒后重试。";
                QThread::msleep(1000);
                continue;
            }
        }

        if (outputCtx != nullptr && !pendingRotate && segmentDurationUs > 0)
        {
            const int64_t segmentWallElapsedUs = duration_cast<microseconds>(SteadyClock::now() - segmentStartWall).count();
            if (segmentWallElapsedUs >= segmentDurationUs)
            {
                pendingRotate = true;
                qDebug() << "达到分段本机时长，等待关键帧切片。";
            }
        }

        const int ret = av_read_frame(inputCtx, packet);
        if (ret == AVERROR(EAGAIN))
        {
            av_packet_unref(packet);
            QThread::msleep(5);
            continue;
        }

        if (ret < 0)
        {
            av_packet_unref(packet);
            if (ret == AVERROR_EOF)
            {
                qDebug() << "RTSP 输入到达 EOF，准备重连。";
            }
            else
            {
                qDebug() << "读取数据包失败，准备重连，错误:" << ffmpegErrorString(ret);
            }

            if (outputCtx != nullptr)
            {
                closeOutputContext(&outputCtx);
                resetSegmentState();
            }

            closeInput();

            if (m_isRunning.load())
            {
                QThread::msleep(1000);
            }
            continue;
        }

        if (packet->stream_index != videoStreamIndex)
        {
            av_packet_unref(packet);
            continue;
        }

        const bool shouldRotateSegment = (!waitingKeyFrame && pendingRotate && (packet->flags & AV_PKT_FLAG_KEY));
        if (shouldRotateSegment)
        {
            closeOutputContext(&outputCtx);
            resetSegmentState();
            qDebug() << "按本机时间切换到新分段文件。";
        }

        if (waitingKeyFrame)
        {
            if (!(packet->flags & AV_PKT_FLAG_KEY))
            {
                av_packet_unref(packet);
                continue;
            }

            manageDiskSpace(outputDir);

            const QString outputFile = nextOutputFilePath(++segmentIndex);
            int           outputErr  = 0;
            if (!createVideoOnlyOutputContext(inputCtx, videoStreamIndex, outputFile, &outputCtx, outputErr))
            {
                av_packet_unref(packet);
                qDebug() << "创建输出文件失败:" << outputFile << "错误:" << ffmpegErrorString(outputErr);
                break;
            }

            waitingKeyFrame   = false;
            segmentStartPts   = packet->pts;
            segmentStartDts   = packet->dts;
            pendingRotate     = false;
            segmentStartWall  = SteadyClock::now();

            qDebug() << "开始写入分段文件:" << outputFile;
        }

        if (outputCtx == nullptr)
        {
            av_packet_unref(packet);
            continue;
        }

        AVStream * inStream  = inputCtx->streams[videoStreamIndex];
        AVStream * outStream = outputCtx->streams[0];

        AVRounding rnd = static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);

        if (packet->pts != AV_NOPTS_VALUE)
        {
            if (segmentStartPts == AV_NOPTS_VALUE)
            {
                segmentStartPts = packet->pts;
            }

            int64_t normalizedPts = packet->pts - segmentStartPts;
            if (normalizedPts < 0)
            {
                normalizedPts = 0;
            }
            packet->pts = normalizedPts;
        }

        if (packet->dts != AV_NOPTS_VALUE)
        {
            if (segmentStartDts == AV_NOPTS_VALUE)
            {
                segmentStartDts = packet->dts;
            }

            int64_t normalizedDts = packet->dts - segmentStartDts;
            if (normalizedDts < 0)
            {
                normalizedDts = 0;
            }
            packet->dts = normalizedDts;
        }

        if (packet->pts == AV_NOPTS_VALUE && packet->dts == AV_NOPTS_VALUE)
        {
            av_packet_unref(packet);
            continue;
        }

        if (packet->pts != AV_NOPTS_VALUE && packet->dts != AV_NOPTS_VALUE && packet->dts > packet->pts)
        {
            packet->dts = packet->pts;
        }

        av_packet_rescale_ts(packet, inStream->time_base, outStream->time_base);
        packet->stream_index = 0;
        packet->pos          = -1;

        const int retWrite = av_interleaved_write_frame(outputCtx, packet);
        av_packet_unref(packet);
        if (retWrite < 0)
        {
            qDebug() << "写入分段文件失败，准备重连，错误:" << ffmpegErrorString(retWrite);
            closeOutputContext(&outputCtx);
            resetSegmentState();
            closeInput();
            QThread::msleep(200);
            continue;
        }
    }

    av_packet_free(&packet);
    closeOutputContext(&outputCtx);
    closeInput();

    m_isRunning.store(false);
    const double wallSeconds = duration_cast<microseconds>(SteadyClock::now() - recordStartWall).count() / static_cast<double>(kMicrosecondsPerSecond);
    qDebug() << "采集线程退出。本机经过时长(秒):" << wallSeconds;
}

bool VideoCaptureManager::validateStartParams(const QString & rtspUrl,
                                              const QString & saveDir,
                                              unsigned        segmentDurationSec,
                                              unsigned        diskThresholdGB,
                                              unsigned        targetDurationSec,
                                              QString &       errorMessage) const
{
    const QString normalizedUrl = rtspUrl.trimmed();
    if (normalizedUrl.isEmpty())
    {
        errorMessage = QStringLiteral("RTSP 地址不能为空。");
        return false;
    }

    if (!normalizedUrl.startsWith(QStringLiteral("rtsp://"), Qt::CaseInsensitive))
    {
        errorMessage = QStringLiteral("仅支持 rtsp:// 开头的地址。");
        return false;
    }

    const QString normalizedDir = saveDir.trimmed();
    if (normalizedDir.isEmpty())
    {
        errorMessage = QStringLiteral("保存目录不能为空。");
        return false;
    }

    if (segmentDurationSec < 1 || segmentDurationSec > 24U * 3600U)
    {
        errorMessage = QStringLiteral("单文件时长必须在 1~86400 秒之间。");
        return false;
    }

    if (diskThresholdGB < 1 || diskThresholdGB > 1024)
    {
        errorMessage = QStringLiteral("磁盘阈值必须在 1~1024 GB 之间。");
        return false;
    }

    return true;
}

bool VideoCaptureManager::prepareSaveDir(const QString & dirPath)
{
    if (dirPath.trimmed().isEmpty())
    {
        return false;
    }

    QDir dir(dirPath);
    if (dir.exists())
    {
        return true;
    }

    return dir.mkpath(".");
}

QString VideoCaptureManager::nextOutputFilePath(int segmentIndex) const
{
    QMutexLocker locker(&m_stateMutex);

    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    const QString fileName  = QString("%1_part_%2.mp4").arg(timestamp).arg(segmentIndex, 4, 10, QChar('0'));

    return QDir(m_outputDir).filePath(fileName);
}

void VideoCaptureManager::manageDiskSpace(const QString & outputDir)
{
    if (m_diskThresholdGB == 0)
    {
        return;
    }

    const qint64 thresholdBytes = static_cast<qint64>(m_diskThresholdGB) * 1024LL * 1024LL * 1024LL;
    qint64       freeBytes      = QStorageInfo(outputDir).bytesFree();

    if (freeBytes >= thresholdBytes)
    {
        return;
    }

    qDebug() << "磁盘剩余空间不足，开始清理旧分段文件。";

    QDir dir(outputDir);
    QFileInfoList files = dir.entryInfoList(QStringList() << "*.mp4", QDir::Files, QDir::Time | QDir::Reversed);

    for (const QFileInfo & file : files)
    {
        if (freeBytes >= thresholdBytes)
        {
            break;
        }

        if (QFile::remove(file.absoluteFilePath()))
        {
            freeBytes = QStorageInfo(outputDir).bytesFree();
            qDebug() << "已删除旧文件:" << file.absoluteFilePath();
        }
    }
}

int VideoCaptureManager::detectFps(AVFormatContext * inputCtx, int videoStreamIndex) const
{
    if (inputCtx == nullptr || videoStreamIndex < 0 || videoStreamIndex >= static_cast<int>(inputCtx->nb_streams))
    {
        return 0;
    }

    AVStream * stream = inputCtx->streams[videoStreamIndex];

    double fps = 0.0;

    const AVRational guessed = av_guess_frame_rate(inputCtx, stream, nullptr);
    if (guessed.num > 0 && guessed.den > 0)
    {
        fps = av_q2d(guessed);
    }

    if (fps <= 0.0 && stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0)
    {
        fps = av_q2d(stream->avg_frame_rate);
    }

    if (fps <= 0.0 && stream->r_frame_rate.num > 0 && stream->r_frame_rate.den > 0)
    {
        fps = av_q2d(stream->r_frame_rate);
    }

    if (fps <= 0.0)
    {
        return 0;
    }

    return static_cast<int>(std::round(fps));
}
