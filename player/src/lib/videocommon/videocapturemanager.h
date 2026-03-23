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
 * @brief RTSP 视频录制管理器（基于 QThread + FFmpeg）。
 *
 * 设计目标：
 * 1. 在独立线程内持续拉取 RTSP 视频流并写入 MP4 文件。
 * 2. 支持按分段时长切片（在关键帧切段，保证文件可播放性）。
 * 3. 支持磁盘阈值清理，避免持续录制导致磁盘写满。
 * 4. 支持目标录制总时长（按墙钟时间控制停止）。
 * 5. 支持网络抖动自动重连。
 */
class VIDEOCOMMON_EXPORT VideoCaptureManager : public QThread
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数。
     * @param parent Qt 父对象指针。
     */
    explicit VideoCaptureManager(QObject * parent = nullptr);

    /**
     * @brief 析构函数。析构时会主动调用 stop() 并等待线程退出。
     */
    ~VideoCaptureManager() override;

    /**
     * @brief 启动视频采集任务。
     * @param rtspUrl RTSP 流地址，必须以 rtsp:// 开头。
     * @param saveDir 录像保存目录，目录不存在时会尝试创建。
     * @param segmentDurationSec 单个分段文件时长（秒），范围 1~86400。
     * @param diskThresholdGB 磁盘剩余空间阈值（GB），低于阈值时会清理旧文件。
     * @param targetDurationSec 目标总录制时长（秒），0 表示不限时长持续录制。
     * @return true 启动成功。
     * @return false 启动失败（参数不合法、目录不可用或线程已在运行）。
     */
    bool start(const QString & rtspUrl,
               const QString & saveDir,
               unsigned        segmentDurationSec,
               unsigned        diskThresholdGB   = 10,
               unsigned        targetDurationSec = 0);

    /**
     * @brief 停止采集任务并等待线程退出。
     */
    void stop();

    /**
     * @brief 旧接口兼容入口：使用默认参数启动录制。
     * @param inputUrl RTSP 流地址。
     */
    void startCapture(const QString & inputUrl);

    /**
     * @brief 旧接口兼容入口：停止录制。
     */
    void stopCapture();

protected:
    /**
     * @brief 线程主函数。
     */
    void run() override;

private:
    /**
     * @brief 校验 start() 参数合法性。
     * @param rtspUrl RTSP 流地址。
     * @param saveDir 保存目录。
     * @param segmentDurationSec 分段时长（秒）。
     * @param diskThresholdGB 磁盘阈值（GB）。
     * @param targetDurationSec 目标总时长（秒）。
     * @param errorMessage 校验失败时输出错误描述。
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
     * @return true 目录可用（存在或创建成功）。
     * @return false 目录不可用。
     */
    bool prepareSaveDir(const QString & dirPath);

    /**
     * @brief 生成下一段 MP4 文件路径。
     * @param segmentIndex 分段序号（从 1 开始）。
     * @return 输出文件完整路径。
     */
    QString nextOutputFilePath(int segmentIndex) const;

    /**
     * @brief 根据磁盘阈值清理旧分段文件。
     * @param outputDir 输出目录。
     */
    void manageDiskSpace(const QString & outputDir);

    /**
     * @brief 检测视频流帧率，优先使用 FFmpeg 推测值。
     * @param inputCtx 输入格式上下文。
     * @param videoStreamIndex 视频流索引。
     * @return 帧率（整数），失败返回 0。
     */
    int detectFps(AVFormatContext * inputCtx, int videoStreamIndex) const;

private:
    mutable QMutex m_stateMutex; ///< 保护 start()/run() 间共享状态（URL、目录、录制参数）。

    QString m_inputUrl;  ///< 当前 RTSP 输入地址（线程启动前写入，run() 中读取）。
    QString m_outputDir; ///< 当前分段文件输出目录。

    int m_fps{30}; ///< 当前检测到的视频帧率，仅用于日志与调试观测。

    unsigned m_segmentDuration{3600}; ///< 单段文件时长（秒）。
    unsigned m_diskThresholdGB{10};   ///< 最低磁盘剩余阈值（GB），低于阈值触发清理。
    unsigned m_targetDuration{0};     ///< 总录制目标时长（秒），0 表示不限制总时长。

    std::atomic<bool> m_isRunning{false}; ///< 运行标志，同时用于 FFmpeg 阻塞读中断回调。
};

#endif // VIDEOCAPTUREMANAGER_H
