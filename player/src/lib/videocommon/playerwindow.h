/*!
 * \file  .\src\lib\videocommon\playerwindow.h.
 *
 * Declares the playerwindow class
 */

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

/*! . */
namespace Ui {

/*!
 * 查看播放器的界面。
 *
 * \author bf
 * \date 2025/8/5
 */

class PlayerWindow;
}

/*!
 * 播放窗口私有成员。
 *
 * \author bf
 * \date 2025/8/5
 */

struct PlayerWindowPrivate;

/*!
 * 播放器窗口
 *
 * \author bf
 * \date 2025/8/5
 */

class VIDEOCOMMON_EXPORT PlayerWindow : public QWidget
{
    Q_OBJECT

public:

    /*!
     * 构造是需要明确 isUseRightMouse ，是否需要启用鼠标右键
     *
     * \author bf
     * \date 2025/8/5
     *
     * \param 		   是否使用右键鼠标
     * \param [in,out] parent (可选)
     */

    explicit PlayerWindow(bool isUseRightMouse = false, QWidget * parent = nullptr);

    /*!
     * 析构<see cref="PlayerWindow"/>类的实例
     *
     * \author bf
     * \date 2025/8/5
     */

    ~PlayerWindow()override;

    /*!
     * 打开视频文件
     *
     * \author bf
     * \date 2025/8/5
     *
     * \param  URL.
     */

    void openUrl(QString url);

protected:

    /*!
     * 上下文菜单事件
     *
     * \author bf
     * \date 2025/8/5
     *
     * \param [in,out] event 若非空，event对象.
     */

    void contextMenuEvent(QContextMenuEvent * event) override;

    void resizeEvent(QResizeEvent * event) override;

/*!
 * 执行“计时器更新”动作
 *
 * \author bf
 * \date 2025/8/5
 */

private slots:
    void onTimerUpdate();

private:
    Ui::PlayerWindow * ui;  ///< Ui界面类

    PlayerWindowPrivate * m_pPrivate{nullptr};  ///< 私有

    PlayerThread * m_pPlayerThread{nullptr};	///< 解码线程
    QTimer *       m_pFrameTimer{nullptr};  ///< 帧定时器，用于定时读取帧图片

    int m_videoWidth{0};	///< 视频宽度
    int m_videoHeight{0};   ///< 视频高度

    int  m_videoStreamIndex{-1};	///< 视频流索引
    bool m_isPlaying{false};	///< 播放状态

    CircularQueue<AVFrame *> m_frameQueue{10};  ///< 帧队列

    CircularQueue<QString> m_StrQueue{30};  ///< 队列

    bool   m_isUseRightMouse{false};	///< 是否使用右键菜单
    int    m_duration{0};   ///< 视频时长（秒）
    int    m_fps{30};   ///< 视频帧率
    double m_currentPosition{0.0};  ///< 当前播放位置（秒）

    unsigned m_frameSpace{40};  ///< 帧间隔（毫秒）

    bool m_spliderPressed{false};   ///< 滑块按下状态

    // QLabel * m_label;
};

#endif // PLAYERWINDOW_H
