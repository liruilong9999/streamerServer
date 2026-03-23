#ifndef VIDEOCAPTUREMANAGER_H
#define VIDEOCAPTUREMANAGER_H

#include <QObject>
#include <QThread>
#include <QString>
#include <QDir>
#include <QFileInfoList>
#include <QMutex>

#include <atomic>

#include "videocommon_global.h"

struct AVFormatContext;

/**
 * @brief RTSP 视频录制管理器。
 *
 * 功能说明：
 * 1. 在独立线程中从 RTSP 拉流并保存 MP4 文件，不阻塞主线程。
 * 2. 自动探测视频流帧率。
 * 3. 按指定时长切片保存（例如 3600 秒一个文件）。
 * 4. 根据磁盘剩余空间阈值自动清理旧文件。
 */
class VIDEOCOMMON_EXPORT VideoCaptureManager : public QThread
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数。
     * @param parent Qt 父对象。
     */
    explicit VideoCaptureManager(QObject * parent = nullptr);

    /**
     * @brief 析构函数，析构前会自动停止采集线程。
     */
    ~VideoCaptureManager() override;

    /**
     * @brief 启动录制任务。
     *
     * 参数通过外部接口传入，不依赖配置文件。
     * 该函数会先做参数合理性校验，校验通过后启动线程。
     *
     * @param rtspUrl RTSP 地址，必须以 rtsp:// 开头。
     * @param saveDir 保存目录，不能为空。
     * @param segmentDurationSec 单个文件时长（秒），范围 1~86400。
     * @param diskThresholdGB 磁盘阈值（GB），范围 1~1024。
     * @param targetDurationSec 目标总录制时长（秒），为 0 表示持续录制直到外部 stop。
     * @return true 启动成功。
     * @return false 启动失败（参数不合法或线程已在运行）。
     */
    bool start(const QString & rtspUrl,
               const QString & saveDir,
               unsigned        segmentDurationSec,
               unsigned        diskThresholdGB  = 10,
               unsigned        targetDurationSec = 0);

    /**
     * @brief 停止录制任务并等待线程退出。
     */
    void stop();

    /**
     * @brief 兼容旧接口：使用默认参数启动录制。
     *
     * 默认参数：
     * - saveDir: video
     * - segmentDurationSec: 3600
     * - diskThresholdGB: 10
     *
     * @param inputUrl RTSP 地址。
     */
    void startCapture(const QString & inputUrl);

    /**
     * @brief 兼容旧接口：停止录制。
     */
    void stopCapture();

protected:
    /**
     * @brief 线程主函数。
     */
    void run() override;

private:
    /**
     * @brief 校验启动参数是否合法。
     * @param rtspUrl RTSP 地址。
     * @param saveDir 保存目录。
     * @param segmentDurationSec 单文件时长（秒）。
     * @param diskThresholdGB 磁盘阈值（GB）。
     * @param targetDurationSec 目标总录制时长（秒），0 表示不限时长。
     * @param errorMessage 错误信息（中文）。
     * @return true 参数合法。
     * @return false 参数不合法。
     */
    bool validateStartParams(const QString & rtspUrl,
                             const QString & saveDir,
                             unsigned        segmentDurationSec,
                             unsigned        diskThresholdGB,
                             unsigned        targetDurationSec,
                             QString &       errorMessage) const;

    /**
     * @brief 确保保存目录存在。
     * @param dirPath 保存目录路径。
     * @return true 目录已存在或创建成功。
     * @return false 创建失败。
     */
    bool prepareSaveDir(const QString & dirPath);

    /**
     * @brief 生成新分段文件路径。
     * @param segmentIndex 分段编号（从 1 开始）。
     * @return 文件路径。
     */
    QString nextOutputFilePath(int segmentIndex) const;

    /**
     * @brief 根据磁盘阈值清理旧文件。
     * @param outputDir 输出目录。
     */
    void manageDiskSpace(const QString & outputDir);

    /**
     * @brief 检测 RTSP 视频流帧率。
     * @param inputCtx 输入格式上下文。
     * @param videoStreamIndex 视频流索引。
     * @return 帧率（整数），失败返回 0。
     */
    int detectFps(AVFormatContext * inputCtx, int videoStreamIndex) const;

private:
    mutable QMutex m_stateMutex; ///< 保护共享状态。

    QString m_inputUrl;  ///< 当前 RTSP 地址。
    QString m_outputDir; ///< 当前保存目录。

    int m_fps{30}; ///< 当前检测帧率（仅日志/统计）。

    unsigned m_segmentDuration{3600}; ///< 单文件时长（秒）。
    unsigned m_diskThresholdGB{10};   ///< 磁盘清理阈值（GB）。
    unsigned m_targetDuration{0};     ///< 总录制时长（秒），0 表示不限时长。

    std::atomic<bool> m_isRunning{false}; ///< 运行标志，同时用于中断 FFmpeg 阻塞读取。
};

#endif // VIDEOCAPTUREMANAGER_H
