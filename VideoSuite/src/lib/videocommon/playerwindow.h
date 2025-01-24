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

class SwsContext;

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
    // 重写 paintEvent 进行绘制
    void paintEvent(QPaintEvent * event) override;
    void contextMenuEvent(QContextMenuEvent * event) override;

private slots:
    void onSliderVideochanged(int pos);
    void onTimerUpdate();

    // 音频暂时不做处理
    //  void onSliderVoicechanged(int pos);
    //  void onBtnPauseClicked();
    //  void onBtnVoiceClicked();

private:
    Ui::PlayerWindow * ui;

    AVFormatContext * m_pFormatContext{nullptr}; // FFmpeg 格式上下文
    AVCodecContext *  m_pCodecContext{nullptr};  // FFmpeg 编解码上下文
    SwsContext *      m_pSwsContext{nullptr};    // 图像转换上下文
    PlayerThread *    m_pPlayerThread{nullptr};  // 解码线程
    QTimer *          m_pFrameTimer{nullptr};    // 帧定时器，用于定时读取帧图片

    int  m_videoStreamIndex{-1}; // 视频流索引
    bool m_isPlaying{false};     // 播放状态

    QImage                m_CurrentImage;  // 当前显示的图像
    CircularQueue<QImage> m_imageQueue{6}; // 帧图片队列

    bool m_isUseRightMouse{false}; // 是否使用右键菜单
    int  m_duration{0};            // 视频时长（秒）
};

#endif // PLAYERWINDOW_H
