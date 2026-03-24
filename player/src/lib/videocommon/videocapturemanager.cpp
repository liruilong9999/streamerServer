/*
 * VideoCaptureManager 录制主流程说明（仅针对本类）：
 * 1. start() 校验参数并缓存运行配置，然后启动线程。
 * 2. run() 在线程内按需建立 RTSP 连接，自动检测视频流与帧率。
 * 3. 按墙钟时间判断是否达到目标总时长；按关键帧进行分段切片。
 * 4. 每次新建分段前执行磁盘阈值检查，必要时删除最旧 MP4 文件。
 * 5. 写入前将 PTS/DTS 归一化到分段局部时间轴，确保每段从 0 附近开始。
 * 6. 遇到网络异常/EOF 自动重连，stop() 或达到目标时长后安全收尾退出。
 */
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

namespace {
constexpr int64_t kMicrosecondsPerSecond = 1000000LL;

// FFmpeg 阻塞读中断回调：当 m_isRunning 为 false 时中断 av_read_frame。
int interruptReadCallback(void * opaque)
{
    auto * running = static_cast<std::atomic<bool> *>(opaque);
    return (running != nullptr && !running->load()) ? 1 : 0;
}

// 将 FFmpeg 错误码转为可读字符串，便于日志定位问题。
QString ffmpegErrorString(int errorCode)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errorCode, errorBuffer, sizeof(errorBuffer));
    return QString::fromLocal8Bit(errorBuffer);
}

// 释放输出上下文（包含可能已打开的 IO 句柄）。
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

// 正常关闭输出：先写 trailer 再释放上下文。
void closeOutputContext(AVFormatContext ** outputCtx)
{
    if (outputCtx == nullptr || *outputCtx == nullptr)
    {
        return;
    }

    av_write_trailer(*outputCtx);
    freeOutputContext(outputCtx);
}

// 仅创建“视频流”输出上下文：复制 codecpar 并准备写 MP4 头。
bool createVideoOnlyOutputContext(AVFormatContext *  inputCtx,
                                  int                videoStreamIndex,
                                  const QString &    outputFile,
                                  AVFormatContext ** outputCtx,
                                  int &              errorCode)
{
    // 这里选择“仅复制视频流”而不转码，目的是最大化稳定性并降低 CPU 占用。
    // 录制场景下优先保证连续落盘，比复杂多流封装更重要。
    errorCode = 0;

    if (inputCtx == nullptr || outputCtx == nullptr || videoStreamIndex < 0 || videoStreamIndex >= static_cast<int>(inputCtx->nb_streams))
    {
        errorCode = AVERROR(EINVAL);
        return false;
    }

    *outputCtx = nullptr;

    const QByteArray outputFileUtf8 = outputFile.toUtf8();
    errorCode                       = avformat_alloc_output_context2(outputCtx, nullptr, nullptr, outputFileUtf8.constData());
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
            freeOutputContext(outputCtx);
            return false;
        }
    }

    errorCode = avformat_write_header(*outputCtx, nullptr);
    if (errorCode < 0)
    {
        freeOutputContext(outputCtx);
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
    // 防止重复启动线程，避免并发写文件与状态竞争。
    if (m_isRunning.load() || QThread::isRunning())
    {
        qDebug() << "采集任务已在运行，不能重复启动。";
        return false;
    }

    QString errorMessage;
    // 参数校验放在线程启动前：失败可立即返回，避免启动后半途报错导致状态复杂化。
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
        // start() 与 run() 通过受保护状态交接参数。
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
             << "分段时长(秒):" << segmentDurationSec
             << "磁盘阈值(GB):" << diskThresholdGB
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
    (void)start(inputUrl, QString("video"), 3600, 10, 0);
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
        // 读取启动阶段缓存的配置快照，避免运行中读写冲突。
        QMutexLocker locker(&m_stateMutex);
        inputUrl           = m_inputUrl;
        outputDir          = m_outputDir;
        segmentDurationSec = m_segmentDuration;
        targetDurationSec  = m_targetDuration;
    }

    AVFormatContext * inputCtx         = nullptr; // 输入 RTSP 上下文（可在重连后替换）。
    AVFormatContext * outputCtx        = nullptr; // 当前分段输出上下文。
    int               videoStreamIndex = -1;      // 输入视频流索引。
    int               segmentIndex     = 0;       // 分段编号，自增用于生成文件名。

    bool    waitingKeyFrame = true;           // 新分段打开后，先等待关键帧再写入。
    bool    pendingRotate   = false;          // 达到分段时长后置位，等关键帧真正切段。
    int64_t segmentStartPts = AV_NOPTS_VALUE; // 当前分段起始 PTS。
    int64_t segmentStartDts = AV_NOPTS_VALUE; // 当前分段起始 DTS。

    const int64_t segmentDurationUs = static_cast<int64_t>(segmentDurationSec) * kMicrosecondsPerSecond;
    const int64_t targetDurationUs =
        (targetDurationSec == 0) ? 0 : static_cast<int64_t>(targetDurationSec) * kMicrosecondsPerSecond;

    const SteadyClock::time_point recordStartWall  = SteadyClock::now();
    SteadyClock::time_point       segmentStartWall = recordStartWall;

    AVPacket * packet = av_packet_alloc();
    if (packet == nullptr)
    {
        qDebug() << "采集线程初始化失败：无法分配 AVPacket。";
        m_isRunning.store(false);
        return;
    }

    auto resetSegmentState = [&]() {
        // 切段后重置局部时间轴与关键帧等待状态。
        waitingKeyFrame  = true;
        pendingRotate    = false;
        segmentStartPts  = AV_NOPTS_VALUE;
        segmentStartDts  = AV_NOPTS_VALUE;
        segmentStartWall = SteadyClock::now();
    };

    auto closeInput = [&]() {
        // 统一输入收尾函数：重连和退出都走同一条路径，减少遗漏释放分支。
        if (inputCtx != nullptr)
        {
            avformat_close_input(&inputCtx);
            inputCtx = nullptr;
        }
        videoStreamIndex = -1;
    };

    auto openInput = [&]() -> bool {
        // 每次重连都重新分配输入上下文，避免复用脏状态。
        inputCtx = avformat_alloc_context();
        if (inputCtx == nullptr)
        {
            qDebug() << "分配输入上下文失败。";
            return false;
        }

        inputCtx->interrupt_callback.callback = interruptReadCallback;
        inputCtx->interrupt_callback.opaque   = &m_isRunning;

        AVDictionary * options = nullptr;
        // RTSP 拉流与时间戳相关选项：
        // 1) 强制 TCP，降低丢包导致的时间轴抖动；
        // 2) 读写超时，避免永久阻塞；
        // 3) use_wallclock_as_timestamps + genpts，尽量稳定输出时间轴。
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
                avformat_close_input(&inputCtx);
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
            // 某些 RTSP 源不会稳定上报帧率，这里给保守默认值避免后续日志/计算异常。
            m_fps = 25;
            qDebug() << "未检测到有效帧率，使用默认帧率 25 fps。";
        }

        qDebug() << "RTSP 已连接。视频流索引:" << videoStreamIndex << "fps:" << m_fps;
        return true;
    };

    // 主循环：时长控制 -> 连接维护 -> 读包 -> 切段 -> 写包。
    while (m_isRunning.load())
    {
        // 目标总时长按墙钟计算，避免输入 PTS 与真实时间不一致。
        const int64_t wallElapsedUs = duration_cast<microseconds>(SteadyClock::now() - recordStartWall).count();
        if (targetDurationUs > 0 && wallElapsedUs >= targetDurationUs)
        {
            qDebug() << "达到目标录制时长(墙钟秒):" << targetDurationSec << "停止录制。";
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
            // 分段时长先按墙钟触发，再等待关键帧真正切段。
            const int64_t segmentWallElapsedUs = duration_cast<microseconds>(SteadyClock::now() - segmentStartWall).count();
            if (segmentWallElapsedUs >= segmentDurationUs)
            {
                pendingRotate = true;
                qDebug() << "达到分段时长，等待关键帧切片。";
            }
        }

        const int ret = av_read_frame(inputCtx, packet);
        if (ret == AVERROR(EAGAIN))
        {
            // 暂时无包，短暂休眠避免空转占用 CPU。
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
                // 输入异常时关闭当前分段，避免生成坏文件。
                closeOutputContext(&outputCtx);
                resetSegmentState();
            }

            // 关闭输入并走重连，原因是网络层错误通常不可通过继续 read_frame 自愈。
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
            // 关键帧到达后执行切段，保证新文件从关键帧开始。
            closeOutputContext(&outputCtx);
            resetSegmentState();
            qDebug() << "切换到新分段文件。";
        }

        if (waitingKeyFrame)
        {
            if (!(packet->flags & AV_PKT_FLAG_KEY))
            {
                // 新分段必须从关键帧开始，避免播放器首帧解码失败或花屏。
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

            waitingKeyFrame  = false;
            pendingRotate    = false;
            segmentStartPts  = packet->pts;
            segmentStartDts  = packet->dts;
            segmentStartWall = SteadyClock::now();

            qDebug() << "开始写入分段文件:" << outputFile;
        }

        if (outputCtx == nullptr)
        {
            av_packet_unref(packet);
            continue;
        }

        AVStream * inStream  = inputCtx->streams[videoStreamIndex];
        AVStream * outStream = outputCtx->streams[0];

        // 时间戳归一化：将每段时间轴平移到从 0 开始，避免跨段时间跳变。
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

        // 最后按输入/输出 time_base 进行重标定后写入文件。
        // 即使当前实现 time_base 相同，也保留该步骤，便于后续容器/参数调整时保持正确。
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
    const double wallSeconds =
        duration_cast<microseconds>(SteadyClock::now() - recordStartWall).count() / static_cast<double>(kMicrosecondsPerSecond);
    qDebug() << "采集线程退出。墙钟耗时(秒):" << wallSeconds;
}

bool VideoCaptureManager::validateStartParams(const QString & rtspUrl,
                                              const QString & saveDir,
                                              unsigned        segmentDurationSec,
                                              unsigned        diskThresholdGB,
                                              unsigned        targetDurationSec,
                                              QString &       errorMessage) const
{
    // 参数校验统一在启动前完成，避免线程中出现可预期失败。
    const QString normalizedUrl = rtspUrl.trimmed();
    if (normalizedUrl.isEmpty())
    {
        errorMessage = QString("RTSP 地址不能为空。");
        return false;
    }

    if (!normalizedUrl.startsWith(QString("rtsp://"), Qt::CaseInsensitive))
    {
        errorMessage = QString("仅支持 rtsp:// 开头的地址。");
        return false;
    }

    const QString normalizedDir = saveDir.trimmed();
    if (normalizedDir.isEmpty())
    {
        errorMessage = QString("保存目录不能为空。");
        return false;
    }

    if (segmentDurationSec < 1 || segmentDurationSec > 24U * 3600U)
    {
        errorMessage = QString("单文件时长必须在 1~86400 秒之间。");
        return false;
    }

    if (diskThresholdGB < 1 || diskThresholdGB > 1024)
    {
        errorMessage = QString("磁盘阈值必须在 1~1024 GB 之间。");
        return false;
    }

    if (targetDurationSec > 24U * 3600U)
    {
        errorMessage = QString("目标总时长不能超过 86400 秒。");
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
        // 阈值为 0 视为禁用磁盘清理策略。
        return;
    }

    // 录制是长时间任务，统一转换为字节阈值后可直接和系统剩余空间比较。
    const qint64 thresholdBytes = static_cast<qint64>(m_diskThresholdGB) * 1024LL * 1024LL * 1024LL;
    qint64       freeBytes      = QStorageInfo(outputDir).bytesFree();

    if (freeBytes >= thresholdBytes)
    {
        return;
    }

    qDebug() << "磁盘剩余空间不足，开始清理旧分段文件。当前空闲字节:" << freeBytes
             << "阈值字节:" << thresholdBytes;

    QDir dir(outputDir);
    // 按时间升序（最旧在前）尝试删除，直到空间恢复到阈值以上。
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
            qDebug() << "已删除旧文件:" << file.absoluteFilePath() << "删除后空闲字节:" << freeBytes;
        }
    }

    if (freeBytes < thresholdBytes)
    {
        qDebug() << "清理后磁盘空间仍低于阈值，可能影响后续录制。当前空闲字节:" << freeBytes;
    }
}

int VideoCaptureManager::detectFps(AVFormatContext * inputCtx, int videoStreamIndex) const
{
    if (inputCtx == nullptr || videoStreamIndex < 0 || videoStreamIndex >= static_cast<int>(inputCtx->nb_streams))
    {
        return 0;
    }

    AVStream * stream = inputCtx->streams[videoStreamIndex];
    double     fps    = 0.0;

    // 优先级：guessed > avg_frame_rate > r_frame_rate。
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
