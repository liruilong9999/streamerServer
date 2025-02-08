#ifndef PLAYERWINDOW_H
#define PLAYERWINDOW_H

#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QLabel>
#include <QTimer>
#include <QContextMenuEvent>

#include "playerthread.h"
#include "videocommon_global.h"

namespace Ui {
class PlayerWindow;
}

struct PlayerWindowPrivate;

/// <summary>
/// 播放器窗口
/// </summary>
class VIDEOCOMMON_EXPORT PlayerWindow : public QWidget
{
    Q_OBJECT

public:
    /// <summary>
    /// 构造是需要明确 isUseRightMouse ，是否需要启用鼠标右键
    /// </summary>
    /// <param name="isUseRightMouse"></param>
    /// <param name="parent"></param>
    explicit PlayerWindow(bool isUseRightMouse = false, QWidget * parent = nullptr);
    ~PlayerWindow();

    /// <summary>
    ///  打开视频文件
    /// </summary>
    /// <param name="url"></param>
    /// <returns></returns>
    bool openUrl(QString url);

protected:
    void contextMenuEvent(QContextMenuEvent * event) override;

private slots:
    void onTimerUpdate();

private:
    Ui::PlayerWindow * ui;

    PlayerWindowPrivate * m_pPrivate{nullptr};

    PlayerThread * m_pPlayerThread{nullptr}; // 解码线程
    QTimer *       m_pFrameTimer{nullptr};   // 帧定时器，用于定时读取帧图片

    int m_videoWidth{0};
    int m_videoHeight{0};

    int  m_videoStreamIndex{-1}; // 视频流索引
    bool m_isPlaying{false};     // 播放状态

    CircularQueue<AVFrame *> m_frameQueue{6}; // 帧队列

    bool   m_isUseRightMouse{false}; // 是否使用右键菜单
    int    m_duration{0};            // 视频时长（秒）
    int    m_fps{30};                // 视频帧率
    double m_currentPosition{0.0};   // 当前播放位置（秒）

    unsigned m_frameSpace{40}; // 帧间隔（毫秒）

    bool m_spliderPressed{false}; // 滑块按下状态
};

#endif // PLAYERWINDOW_H
