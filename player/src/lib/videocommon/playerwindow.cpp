/*!
 * \file  .\src\lib\videocommon\playerwindow.cpp.
 *
 * Implements the playerwindow class.
 */

#include <QFileDialog>
#include <QInputDialog>
#include <QMenu>

#include <SDL2/SDL.h>

extern "C"
{
#include <libavutil/frame.h>
}

#include "ui_playerwindow.h"

#include "playerwindow.h"

struct PlayerWindowPrivate
{
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

    ui->widget->setAttribute(Qt::WA_PaintOnScreen, true);
    ui->widget->setAttribute(Qt::WA_NoSystemBackground, true);
    ui->widget->setAttribute(Qt::WA_OpaquePaintEvent, true);
    ui->widget->setUpdatesEnabled(false);

    SDL_Init(SDL_INIT_VIDEO);
    m_pPrivate->m_sdlWindow   = SDL_CreateWindowFrom(reinterpret_cast<void *>(ui->widget->winId()));
    m_pPrivate->m_sdlRenderer = SDL_CreateRenderer(m_pPrivate->m_sdlWindow, -1, SDL_RENDERER_ACCELERATED);

    m_pPlayerThread = new PlayerThread(m_frameQueue);

    m_pFrameTimer = new QTimer(this);
    connect(m_pFrameTimer, &QTimer::timeout, this, &PlayerWindow::onTimerUpdate);

    connect(
        m_pPlayerThread, &PlayerThread::openSuccess, this, [&](QString, int duration, int fps) {
            m_duration = duration;
            m_fps      = (fps > 0) ? fps : 25;
            m_pFrameTimer->setInterval(1000 / m_fps);
            m_pFrameTimer->start();
        },
        Qt::QueuedConnection);

    connect(
        m_pPlayerThread, &PlayerThread::videoStopped, this, [&]() {
            m_pFrameTimer->stop();
        },
        Qt::QueuedConnection);
}

PlayerWindow::~PlayerWindow()
{
    if (m_pFrameTimer)
    {
        m_pFrameTimer->stop();
    }

    if (m_pPlayerThread)
    {
        delete m_pPlayerThread;
        m_pPlayerThread = nullptr;
    }

    if (m_pPrivate)
    {
        if (m_pPrivate->m_sdlTexture)
        {
            SDL_DestroyTexture(m_pPrivate->m_sdlTexture);
            m_pPrivate->m_sdlTexture = nullptr;
        }
        if (m_pPrivate->m_sdlRenderer)
        {
            SDL_DestroyRenderer(m_pPrivate->m_sdlRenderer);
            m_pPrivate->m_sdlRenderer = nullptr;
        }
    }

    SDL_Quit();

    delete m_pPrivate;
    m_pPrivate = nullptr;

    delete ui;
    ui = nullptr;
}

void PlayerWindow::openUrl(QString url)
{
    m_pFrameTimer->stop();
    m_frameQueue.clear();

    m_pPlayerThread->openUrl(url, m_duration, m_fps);
}

void PlayerWindow::contextMenuEvent(QContextMenuEvent * event)
{
    if (!m_isUseRightMouse)
    {
        return;
    }

    QAction * actionLocal = new QAction(QString("打开本地文件"), this);
    QAction * actionRtsp  = new QAction(QString("打开网络流"), this);

    QMenu menu(this);
    menu.addAction(actionLocal);
    menu.addAction(actionRtsp);

    QAction * act = menu.exec(event->globalPos());
    if (act == actionLocal)
    {
        const QString url = QFileDialog::getOpenFileName(this, QString("打开本地文件"), QString(), QString("媒体文件 (*.mp4 *.flv *.mkv)"));
        if (!url.isEmpty())
        {
            openUrl(url);
        }
    }
    else if (act == actionRtsp)
    {
        const QString url = QInputDialog::getText(this, QString("打开网络流"), QString("请输入流地址:"));
        if (!url.isEmpty())
        {
            openUrl(url);
        }
    }

    delete actionLocal;
    delete actionRtsp;
}

void PlayerWindow::resizeEvent(QResizeEvent * event)
{
    QWidget::resizeEvent(event);
    if (m_pPrivate && m_pPrivate->m_sdlWindow)
    {
        SDL_SetWindowSize(m_pPrivate->m_sdlWindow, ui->widget->width(), ui->widget->height());
    }
}

void PlayerWindow::onTimerUpdate()
{
    AVFrame * frame = nullptr;
    if (!m_frameQueue.tryDequeue(frame))
    {
        return;
    }

    if (frame == nullptr)
    {
        return;
    }

    if ((m_pPrivate->m_sdlTexture == nullptr) || (m_videoWidth != frame->width) || (m_videoHeight != frame->height))
    {
        if (m_pPrivate->m_sdlTexture)
        {
            SDL_DestroyTexture(m_pPrivate->m_sdlTexture);
            m_pPrivate->m_sdlTexture = nullptr;
        }

        m_videoWidth  = frame->width;
        m_videoHeight = frame->height;
        m_pPrivate->m_sdlTexture = SDL_CreateTexture(
            m_pPrivate->m_sdlRenderer,
            SDL_PIXELFORMAT_YV12,
            SDL_TEXTUREACCESS_STREAMING,
            m_videoWidth,
            m_videoHeight);
    }

    if (m_pPrivate->m_sdlTexture)
    {
        SDL_UpdateYUVTexture(
            m_pPrivate->m_sdlTexture,
            nullptr,
            frame->data[0],
            frame->linesize[0],
            frame->data[1],
            frame->linesize[1],
            frame->data[2],
            frame->linesize[2]);

        SDL_RenderClear(m_pPrivate->m_sdlRenderer);
        SDL_RenderCopy(m_pPrivate->m_sdlRenderer, m_pPrivate->m_sdlTexture, nullptr, nullptr);
        SDL_RenderPresent(m_pPrivate->m_sdlRenderer);
    }

    av_frame_free(&frame);
}
