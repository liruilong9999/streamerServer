#ifndef VIDEOCAPTUREMANAGER_H
#define VIDEOCAPTUREMANAGER_H

#include <QObject>
#include <QThread>
#include <QString>
#include <QSettings>
#include <QDir>
#include <QFileInfoList>

#include "videocommon_global.h"

class VIDEOCOMMON_EXPORT VideoCaptureManager : public QThread
{
    Q_OBJECT

public:
    explicit VideoCaptureManager(QObject * parent = nullptr);
    ~VideoCaptureManager();

    void startCapture(const QString & inputUrl);
    void stopCapture();

protected:
    void run() override;

private:
    void   manageDiskSpace(const QString & outputDir);
    qint64 getDirectorySize(const QString & path);

    QString m_inputUrl;  // 输入视频url
    QString m_outputDir; // 保存视频文件夹

    int m_fps{30}; // 帧率

    unsigned m_segmentDuration{60}; // 视频时长（秒）
    unsigned m_diskThresholdGB{10}; // 磁盘空间阈值（GB）

    std::atomic<bool> m_isRunning{false};
};

#endif // VIDEOCAPTUREMANAGER_H
