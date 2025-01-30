#ifndef PLAYERTHREAD_H
#define PLAYERTHREAD_H

#include <QThread>
#include <QImage>

#include <mutex>
#include <condition_variable>
#include <atomic>

#include "circularqueue.h"

class AVFrame;
class AVCodecContext;
class AVFormatContext;
class AVCodec;

class PlayerThread : public QThread
{
    Q_OBJECT
public:
    explicit PlayerThread(CircularQueue<QImage> & imageQueue, QObject * parent = nullptr);
    ~PlayerThread();

    bool openUrl(QString url, int & duration); // 打开文件
    void stop();

public slots:
    void seeToTime(int percent); // 跳转到指定时间点

protected:
    void run() override;

private:
    void   decode(); // 解码 所有帧
    QImage convertFrameToQImage(AVFrame * pFrame, AVCodecContext * pCodecContext);

private:
    AVFormatContext * m_pFormatContext = nullptr;
    AVCodecContext *  m_pCodecContext  = nullptr;
    AVCodec *         m_pCodec         = nullptr;

    CircularQueue<QImage> & m_imageQueue; // 帧图片队列

    int              m_videoStreamIndex = -1;
    std::string      m_filePath;
    std::atomic<int> m_seekTime{-1}; // 跳转到指定时间点(微秒)
    unsigned         m_duration = 0; // 视频总时长(微秒)
};

#endif // PLAYERTHREAD_H
