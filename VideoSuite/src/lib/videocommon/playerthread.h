#ifndef PLAYERTHREAD_H
#define PLAYERTHREAD_H

#include <QThread>
#include <QImage>

#include <mutex>
#include <condition_variable>
#include <atomic>

#include "circularqueue.h"

struct AVFrame;
struct AVCodecContext;
struct AVFormatContext;
struct AVCodec;

class PlayerThread : public QThread
{
    Q_OBJECT
public:
    explicit PlayerThread(CircularQueue<AVFrame *> & frameQueue, QObject * parent = nullptr);
    ~PlayerThread();

    bool openUrl(QString url, int & duration, int & fps); // 打开文件
    void stop();

signals:
    void videoStopped(); // 停止播放

public slots:
    void seeToTime(double percent); // 跳转到指定时间点

protected:
    void run() override;

private:
    void decode(); // 解码 所有帧

    bool openUrlRTSP(QString url, int & duration, int & fps); // 打开文件

private:
    AVFormatContext * m_pFormatContext = nullptr;
    AVCodecContext *  m_pCodecContext  = nullptr;
    AVCodec *         m_pCodec         = nullptr;

    CircularQueue<AVFrame *> & m_frameQueue; // 帧队列

    int              m_videoStreamIndex = -1;
    std::string      m_filePath;
    std::atomic<int> m_seekTime{-1};     // 跳转到指定时间点(微秒)
    unsigned         m_duration = 0;     // 视频总时长(微秒)
    bool             m_isOpened = false; // 是否打开文件
};

#endif // PLAYERTHREAD_H
