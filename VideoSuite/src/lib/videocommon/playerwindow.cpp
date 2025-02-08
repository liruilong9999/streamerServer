
#include <QDebug>
#include <QMenu>
#include <QEvent>

#include <QFileDialog>
#include <QInputDialog>

#include <QTime>

#include <SDL2/SDL.h>

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

struct PlayerWindowPrivate
{
    // 添加SDL相关成员
    SDL_Window *   m_sdlWindow{nullptr};
    SDL_Renderer * m_sdlRenderer{nullptr};
    SDL_Texture *  m_sdlTexture{nullptr};
};

PlayerWindow::PlayerWindow(bool isUseRightMouse, QWidget * parent)
    : QWidget(parent)
    , ui(new Ui::PlayerWindow)
    , m_pPrivate(new PlayerWindowPrivate)
    , m_isUseRightMouse(isUseRightMouse)
{
    ui->setupUi(this);

    // 初始化SDL
    SDL_Init(SDL_INIT_VIDEO);

    // 创建SDL窗口（嵌入到Qt窗口中）
    m_pPrivate->m_sdlWindow   = SDL_CreateWindowFrom((void *)(ui->widget->winId()));
    m_pPrivate->m_sdlRenderer = SDL_CreateRenderer(m_pPrivate->m_sdlWindow, -1, SDL_RENDERER_ACCELERATED);

    connect(ui->sliderVideo, &QSlider::sliderReleased, this, [&]() {
        double pos = ui->sliderVideo->value() / (double)ui->sliderVideo->maximum();
        m_pPlayerThread->seeToTime(pos);
        m_currentPosition = pos * m_duration * 1000;
        m_spliderPressed  = false;
    });

    connect(ui->sliderVideo, &QSlider::sliderPressed, this, [&]() {
        m_spliderPressed = true;
    });

    m_pPlayerThread = new PlayerThread(m_frameQueue, this);

    m_pFrameTimer = new QTimer(this);
    connect(m_pFrameTimer, &QTimer::timeout, this, &PlayerWindow::onTimerUpdate);
    connect(m_pPlayerThread, &PlayerThread::videoStopped, this, [&]() {
        m_pPlayerThread->stop();
        m_pFrameTimer->stop();
        m_currentPosition = 0;
        ui->sliderVideo->setValue(0);
        ui->label->setText("00:00:00");
        // SDL暂停
        SDL_PauseAudio(1);
    });
}

PlayerWindow::~PlayerWindow()
{
    m_pPlayerThread->stop();

    if (m_pPrivate->m_sdlTexture)
        SDL_DestroyTexture(m_pPrivate->m_sdlTexture);
    if (m_pPrivate->m_sdlRenderer)
        SDL_DestroyRenderer(m_pPrivate->m_sdlRenderer);
    SDL_Quit();

    delete ui;
}

bool PlayerWindow::openUrl(QString url)
{
    bool ret = m_pPlayerThread->openUrl(url, m_duration, m_fps);
    if (ret)
    {
        m_currentPosition = 0;
        m_frameSpace      = 1000.0 / m_fps;
        m_pFrameTimer->setInterval(1000 / m_frameSpace);
        m_pFrameTimer->start();
    }

    int hours            = m_duration / 3600;        // 计算小时
    int minutes          = (m_duration % 3600) / 60; // 计算分钟
    int remainingSeconds = m_duration % 60;          // 计算剩余的秒数

    QTime time(hours, minutes, remainingSeconds);

    ui->label_duration->setText(time.toString("H:mm:ss"));
    return ret;
}

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

void PlayerWindow::onTimerUpdate()
{
    AVFrame * frame = nullptr;
    if (m_frameQueue.dequeue(frame))
    {
        // 首次初始化纹理
        if (!m_pPrivate->m_sdlTexture || m_videoWidth != frame->width || m_videoHeight != frame->height)
        {
            if (m_pPrivate->m_sdlTexture)
                SDL_DestroyTexture(m_pPrivate->m_sdlTexture);
            m_videoWidth             = frame->width;
            m_videoHeight            = frame->height;
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
        SDL_RenderCopy(m_pPrivate->m_sdlRenderer, m_pPrivate->m_sdlTexture, nullptr, nullptr);
        SDL_RenderPresent(m_pPrivate->m_sdlRenderer);

        av_frame_free(&frame); // 释放帧内存
    }
    if (m_duration > 0)
    {
        m_currentPosition += m_frameSpace;
        int      currentPos = m_currentPosition / 1000;
        unsigned pos        = ((double)(m_currentPosition / 1000) / (double)m_duration) * ui->sliderVideo->maximum();

        if (!m_spliderPressed)
        {
            ui->sliderVideo->setValue(pos);

            int hours            = currentPos / 3600;        // 计算小时
            int minutes          = (currentPos % 3600) / 60; // 计算分钟
            int remainingSeconds = currentPos % 60;          // 计算剩余的秒数

            QTime time(hours, minutes, remainingSeconds);

            ui->label->setText(time.toString("H:mm:ss"));

            if (currentPos >= m_duration - 1)
            {
                m_pPlayerThread->stop();
                m_pFrameTimer->stop();
                m_currentPosition = 0;
                // SDL暂停
                SDL_PauseAudio(1);
            }
        }
    }
}
