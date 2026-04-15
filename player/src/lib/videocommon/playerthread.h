/*!
 * \file  .\src\lib\videocommon\playerthread.h.
 *
 * Declares the playerthread class
 */

#ifndef PLAYERTHREAD_H
#define PLAYERTHREAD_H

#include <QThread>
#include <QImage>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

#include "circularqueue.h"
#include "videocommon_global.h"

struct AVFrame;
struct AVCodecContext;
struct AVFormatContext;
struct AVCodec;

class VIDEOCOMMON_EXPORT PlayerThread : public QThread
{
    Q_OBJECT
public:
    explicit PlayerThread(CircularQueue<AVFrame *> & frameQueue, QObject * parent = nullptr);

    ~PlayerThread() override;

    void openUrl(QString url, int duration, int fps);
    void stop();

signals:
    void videoStopped();
    void reconnect();
    void openSuccess(QString url, int duration, int fps);

public slots:
    void onReconnect();

protected:
    void run() override;

private:
    void decode();
    bool openUrlRTSP(QString url, int & duration, int & fps);

private:
    AVFormatContext * m_pFormatContext = nullptr;
    AVCodecContext *  m_pCodecContext  = nullptr;
    AVCodec *         m_pCodec         = nullptr;

    CircularQueue<AVFrame *> & m_frameQueue;

    int               m_videoStreamIndex = -1;
    std::string       m_filePath;
    std::atomic<bool> m_stopRequested{false};
    int               m_duration = 0;
    bool              m_isOpened = false;

    QString m_url;
    int     m_fps = 0;
};

#endif // PLAYERTHREAD_H
