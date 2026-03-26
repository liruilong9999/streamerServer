/*!
 * \file  .\src\lib\videocommon\playerthread.h.
 *
 * Declares the playerthread class
 */

#ifndef PLAYERTHREAD_H
#define PLAYERTHREAD_H

#include <QThread>
#include <QImage>

#include <mutex>
#include <condition_variable>
#include <atomic>

#include "circularqueue.h"

/*!
 * av帧.
 *
 * \author bf
 * \date 2025/8/5
 */

struct AVFrame;

/*!
 * 音频/视频编解码器上下文
 *
 * \author bf
 * \date 2025/8/5
 */

struct AVCodecContext;

/*!
 * 音频/视频格式上下文
 *
 * \author bf
 * \date 2025/8/5
 */

struct AVFormatContext;

/*!
 * 音频/视频编解码器
 *
 * \author bf
 * \date 2025/8/5
 */

struct AVCodec;

/*!
 * 播放线程。
 *
 * \author bf
 * \date 2025/8/5
 */

class PlayerThread : public QThread
{
    Q_OBJECT
public:
    /*!
     * 初始化<see cref="PlayerThread"/>类的新实例
     *
     * \author bf
     * \date 2025/8/5
     *
     * \param [in,out] frameQueue [in,out] 若非空，帧队列.
     * \param [in,out] strQueue 队列。
     * \param [in,out] parent (可选) 如果非空，则为父对象。
     */

    explicit PlayerThread(CircularQueue<AVFrame *> & frameQueue, CircularQueue<QString> & strQueue, QObject * parent = nullptr);

    /*!
     * 析构<see cref="PlayerThread"/>类的实例
     *
     * \author bf
     * \date 2025/8/5
     */

    ~PlayerThread();

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

    void openUrl(QString url, int duration, int fps); // 打开文件

    /*!
     * 停止此对象
     *
     * \author bf
     * \date 2025/8/5
     */

    void stop();

    /*!
     * 视频已停止
     *
     * \author bf
     * \date 2025/8/5
     */

signals:
    void videoStopped(); // 停止播放

    /*!
     * 重新连接此对象
     *
     * \author bf
     * \date 2025/8/5
     */

    void reconnect(); // 重新连接

    /*!
     * 打开成功。
     *
     * \author bf
     * \date 2025/8/5
     *
     * \param  文档URL.
     * \param  duration 持续时间
     * \param  fps 帧率。
     */

    void openSuccess(QString url, int duration, int fps);

    /*!
     * 处理时间。
     *
     * \author bf
     * \date 2025/8/5
     *
     * \param  百分比.
     */

public slots:
    void seeToTime(double percent); // 跳转到指定时间点

    /*!
     * 执行 '重新连接' 动作
     *
     * \author bf
     * \date 2025/8/5
     */

    void onReconnect();

protected:
    /*!
     * 运行此对象
     *
     * \author bf
     * \date 2025/8/5
     */

    void run() override;

private:
    /*!
     * 解码此对象。
     *
     * \author bf
     * \date 2025/8/5
     */

    void decode(); // 解码 所有帧

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

    bool openUrlRTSP(QString url, int & duration, int & fps); // 打开文件

private:
    AVFormatContext * m_pFormatContext = nullptr; ///< 格式上下文
    AVCodecContext *  m_pCodecContext  = nullptr; ///< 编解码器上下文
    AVCodec *         m_pCodec         = nullptr; ///< 编解码器

    // 字幕相关
//    AVCodecContext * m_pSubtitleCodecContext = nullptr; ///< 👈 这里
//    int              m_subtitleStreamIndex   = -1;      ///< 👈 这里

    CircularQueue<AVFrame *> & m_frameQueue; ///< 帧队列
  // CircularQueue<QString> &   m_StrQueue;   ///< 帧队列

    int              m_videoStreamIndex = -1; ///< 视频流索引
    std::string      m_filePath;              ///< 文件的完整路径名
    std::atomic<int> m_seekTime{-1};          ///< 跳转到指定时间点(微秒)
    int              m_duration = 0;          ///< 视频总时长(微秒)
    bool             m_isOpened = false;      ///< 是否打开文件

    QString m_url; ///< _URL 文档

    int m_fps; ///< 帧率
};

#endif // PLAYERTHREAD_H
