
#include <QDebug>
#include <QMenu>
#include <QEvent>

#include <QFileDialog>
#include <QInputDialog>

#include <QTime>

#include "ui_playerwindow.h"

#include "playerwindow.h"

PlayerWindow::PlayerWindow(bool isUseRightMouse, QWidget * parent)
    : QWidget(parent)
    , ui(new Ui::PlayerWindow)
    , m_isUseRightMouse(isUseRightMouse)
{
    ui->setupUi(this);

    connect(ui->sliderVideo, &QSlider::valueChanged, this, &PlayerWindow::onSliderVideochanged);
    // connect(ui->sliderVoice, &QSlider::valueChanged, this, &PlayerWindow::onSliderVoicechanged);
    // connect(ui->btnPause, &QPushButton::clicked, this, &PlayerWindow::onBtnPauseClicked);
    // connect(ui->btnVoice, &QPushButton::clicked, this, &PlayerWindow::onBtnVoiceClicked);

    m_pPlayerThread = new PlayerThread(m_imageQueue, this);

    m_pFrameTimer = new QTimer(this);
    connect(m_pFrameTimer, &QTimer::timeout, this, &PlayerWindow::onTimerUpdate);
    m_pFrameTimer->start(1000 / 40);

    ui->btnPause->hide();
    ui->btnVoice->hide();
    ui->sliderVoice->hide();
}

PlayerWindow::~PlayerWindow()
{
    m_pPlayerThread->stop();

    delete ui;
}

bool PlayerWindow::openUrl(QString url)
{
    bool ret = m_pPlayerThread->openUrl(url, m_duration);

    int hours            = m_duration / 3600;        // 计算小时
    int minutes          = (m_duration % 3600) / 60; // 计算分钟
    int remainingSeconds = m_duration % 60;          // 计算剩余的秒数

    QTime time(hours, minutes, remainingSeconds);

    ui->label_duration->setText(time.toString("H:mm:ss"));

    return ret;
}

void PlayerWindow::paintEvent(QPaintEvent * event)
{
    QPainter painter(this);

    // 使用窗口的大小作为目标矩形
    QRect targetRect(0, 0, width(), height());

    // 绘制图像，自动缩放到目标矩形
    painter.drawImage(targetRect, m_CurrentImage);
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

void PlayerWindow::onSliderVideochanged(int pos)
{
    m_pPlayerThread->seeToTime(pos);
}

// void PlayerWindow::onSliderVoicechanged(int pos)
//{
// }
//
// void PlayerWindow::onBtnPauseClicked()
//{
//     QString url1 = "D:\\code\\git\\streamerServer\\live555_server\\bin\\vedio\\test4.mp4";
//     QString url2 = "rtsp://127.0.0.1/vedio/test4.mp4";
//     m_pPlayerThread->openUrl(url2);
// }
//
// void PlayerWindow::onBtnVoiceClicked()
//{
// }

void PlayerWindow::onTimerUpdate()
{
    int size = m_imageQueue.getSize();
    if (size > 0 && m_imageQueue.isStopped() == false)
    {
        m_imageQueue.dequeue(m_CurrentImage);
        update();
    }
}
