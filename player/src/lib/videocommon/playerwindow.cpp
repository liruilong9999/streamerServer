/*!
 * \file  .\src\lib\videocommon\playerwindow.cpp.
 *
 * Implements the playerwindow class
 */

#include <QDebug>
#include <QMenu>
#include <QEvent>

#include <QFileDialog>
#include <QInputDialog>

#include <QTime>

#include <SDL2/SDL.h>

#include <QLabel>

extern "C"
{
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "ui_playerwindow.h"

#include "playerwindow.h"

/*!
 * 播放窗口私有成员。
 *
 * \author bf
 * \date 2025/8/5
 */

struct PlayerWindowPrivate
{
    // 添加SDL相关成员
    SDL_Window *   m_sdlWindow{nullptr};	///< SDL 窗口
    SDL_Renderer * m_sdlRenderer{nullptr};  ///< SDL 渲染器
    SDL_Texture *  m_sdlTexture{nullptr};   ///< SDL 纹理
};

/*!
 * 初始化<see cref="PlayerWindow"/>类的新实例
 *
 * \author bf
 * \date 2025/8/5
 *
 * \param 		   如果此对象使用右键鼠标
 * \param [in,out] parent 如果非空，则为父对象。
 */

PlayerWindow::PlayerWindow(bool isUseRightMouse, QWidget * parent)
    : QWidget(parent)
    , ui(new Ui::PlayerWindow)
    , m_pPrivate(new PlayerWindowPrivate)
    , m_isUseRightMouse(isUseRightMouse)
{
    ui->setupUi(this);

    setMouseTracking(false);

    ui->widget->setAttribute(Qt::WA_PaintOnScreen,true);
    ui->widget->setAttribute(Qt::WA_NoSystemBackground,true);
    ui->widget->setAttribute(Qt::WA_OpaquePaintEvent,true);

    ui->widget->setUpdatesEnabled(false);

    // 初始化SDL
    SDL_Init(SDL_INIT_VIDEO);

    // 创建SDL窗口（嵌入到Qt窗口中）
    m_pPrivate->m_sdlWindow   = SDL_CreateWindowFrom((void *)(ui->widget->winId()));
    m_pPrivate->m_sdlRenderer = SDL_CreateRenderer(m_pPrivate->m_sdlWindow, -1, SDL_RENDERER_ACCELERATED);

    // connect(ui->sliderVideo, &QSlider::sliderReleased, this, [&]() {
    //     double pos = ui->sliderVideo->value() / (double)ui->sliderVideo->maximum();
    //     m_pPlayerThread->seeToTime(pos);
    //     m_currentPosition = pos * m_duration * 1000;
    //     m_spliderPressed  = false;
    // });

    // connect(ui->sliderVideo, &QSlider::sliderPressed, this, [&]() {
    //     m_spliderPressed = true;
    // });

    m_pPlayerThread = new PlayerThread(m_frameQueue, m_StrQueue);

    m_pFrameTimer = new QTimer(this);
    connect(m_pFrameTimer, &QTimer::timeout, this, &PlayerWindow::onTimerUpdate);
    connect(
        m_pPlayerThread, &PlayerThread::openSuccess, this, [&](QString url, int duration, int fps) {
            m_duration = duration;
            m_fps      = fps;

            m_currentPosition = 0;
            m_frameSpace      = 1000.0 / m_fps;
            m_pFrameTimer->setInterval(1000 / m_frameSpace);
            m_pFrameTimer->start();
        },
        Qt::QueuedConnection);
    // connect(m_pPlayerThread, &PlayerThread::videoStopped, this, [&]() {
    //     m_pPlayerThread->stop();
    //     m_pFrameTimer->stop();
    //     m_currentPosition = 0;
    //     //ui->sliderVideo->setValue(0);
    //     //ui->label->setText("00:00:00");
    //     // SDL暂停
    //     SDL_PauseAudio(1);
    // });

    // m_label = new QLabel(this);

    // m_label->setGeometry(0, 0, 1500, 300);
    // QTimer * timerstr = new QTimer(this);
    // connect(timerstr, &QTimer::timeout, this, [&]() {
    //     QString str;
    //     if (!m_StrQueue.isEmpty())
    //     {
    //         m_StrQueue.dequeue(str);
    //         m_label->setText(str);
    //     }
    // });
    // timerstr->start(30);
}

/*!
 * 析构<see cref="PlayerWindow"/>类的实例
 *
 * \author bf
 * \date 2025/8/5
 */

PlayerWindow::~PlayerWindow()
{
    m_pFrameTimer->stop();

    if (m_pPrivate->m_sdlTexture)
         {SDL_DestroyTexture(m_pPrivate->m_sdlTexture);}
    if (m_pPrivate->m_sdlRenderer)
        { SDL_DestroyRenderer(m_pPrivate->m_sdlRenderer);}
    SDL_Quit();

    if (m_pPlayerThread)
    {
        delete m_pPlayerThread;
        m_pPlayerThread = nullptr;
    }

    delete ui;
}

/*!
 * 打开URL。
 *
 * \author bf
 * \date 2025/8/5
 *
 * \param  文档URL.
 */

void PlayerWindow::openUrl(QString url)
{
    m_pPlayerThread->openUrl(url, m_duration, m_fps);

    // int hours            = m_duration / 3600;        // 计算小时
    // int minutes          = (m_duration % 3600) / 60; // 计算分钟
    // int remainingSeconds = m_duration % 60;          // 计算剩余的秒数

    // QTime time(hours, minutes, remainingSeconds);

    // ui->label_duration->setText(time.toString("H:mm:ss"));//
}

/*!
 * 上下文菜单事件
 *
 * \author bf
 * \date 2025/8/5
 *
 * \param [in,out] event 若非空，event对象.
 */

void PlayerWindow::contextMenuEvent(QContextMenuEvent * event)
{
    if (m_isUseRightMouse)
    {
        QAction * action  = new QAction(QString("打开本地文件"), this);
        QAction * action2 = new QAction(QString("打开网络流"), this);
        QMenu     menu(this);
        menu.addAction(action);
        menu.addAction(action2);

        QAction * act = menu.exec(event->globalPos());
        if (act == action)
        {
            QString url = QFileDialog::getOpenFileName(this, "打开本地文件", "", "媒体文件 (*.mp4)");
            if (!url.isEmpty())
            {
                openUrl(url);
            }
        }
        else if (act == action2)
        {
            QString url = QInputDialog::getText(this, "打开网络流", "请输入流地址:");
            if (!url.isEmpty())
            {
                openUrl(url);
            }
        }

        delete action;
        action = nullptr;
        delete action2;
        action2 = nullptr;
    }
}

void PlayerWindow::resizeEvent(QResizeEvent *event)
{
   // qDebug()<<"++++++++++++++++++++++++"<<size();
    QWidget::resizeEvent(event);
    SDL_SetWindowSize(m_pPrivate->m_sdlWindow,ui->widget->width(),ui->widget->height());
}

/*!
 * 执行“计时器更新”动作
 *
 * \author bf
 * \date 2025/8/5
 */

void PlayerWindow::onTimerUpdate()
{
    AVFrame * frame = nullptr;
    if (m_frameQueue.dequeue(frame))
    {
        // 首次初始化纹理
        if ((nullptr == m_pPrivate->m_sdlTexture) || (m_videoWidth != frame->width) || (m_videoHeight != frame->height))
        {
            if (m_pPrivate->m_sdlTexture)
               { SDL_DestroyTexture(m_pPrivate->m_sdlTexture);}
            m_videoWidth             = frame->width;
            m_videoHeight            = frame->height;
            qDebug()<<"============="<<m_videoWidth<<m_videoHeight;
            m_pPrivate->m_sdlTexture = SDL_CreateTexture(
                m_pPrivate->m_sdlRenderer,
                SDL_PIXELFORMAT_YV12,
                SDL_TEXTUREACCESS_STREAMING,
                m_videoWidth,
                m_videoHeight);
        }

        // 更新纹理数据
        SDL_UpdateYUVTexture(
            m_pPrivate->m_sdlTexture,
            nullptr,
            frame->data[0],
            frame->linesize[0],
            frame->data[1],
            frame->linesize[1],
            frame->data[2],
            frame->linesize[2]);

        // 渲染到窗口
        SDL_RenderClear(m_pPrivate->m_sdlRenderer);


//        SDL_Rect sdlRect{0,0,ui->widget->width(),ui->widget->height()};

        SDL_RenderCopy(m_pPrivate->m_sdlRenderer, m_pPrivate->m_sdlTexture, nullptr, nullptr);
        SDL_RenderPresent(m_pPrivate->m_sdlRenderer);

        av_frame_free(&frame); // 释放帧内存
    }
    // if (m_duration > 0)
    //{
    //     m_currentPosition += m_frameSpace;
    //     int      currentPos = m_currentPosition / 1000;
    //     unsigned pos        = ((double)(m_currentPosition / 1000) / (double)m_duration) * ui->sliderVideo->maximum();

    //    if (!m_spliderPressed)
    //    {
    //        ui->sliderVideo->setValue(pos);

    //        int hours            = currentPos / 3600;        // 计算小时
    //        int minutes          = (currentPos % 3600) / 60; // 计算分钟
    //        int remainingSeconds = currentPos % 60;          // 计算剩余的秒数

    //        QTime time(hours, minutes, remainingSeconds);

    //        ui->label->setText(time.toString("H:mm:ss"));

    //        if (currentPos >= m_duration - 1)
    //        {
    //            m_pPlayerThread->stop();
    //            m_pFrameTimer->stop();
    //            m_currentPosition = 0;
    //            // SDL暂停
    //            SDL_PauseAudio(1);
    //        }
    //    }
    //}
}
