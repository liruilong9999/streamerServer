#include "mediayplayerwidget.h"
#include "ui_mediayplayerwidget.h"

#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QPushButton>

MediayPlayerWidget::MediayPlayerWidget(QWidget * parent)
    : QWidget(parent)
    , ui(new Ui::MediayPlayerWidget)
{
    ui->setupUi(this);
    m_vlcInstance     = libvlc_new(0, nullptr);
    m_vlc_mediaPlayer = libvlc_media_player_new(m_vlcInstance);
    // 初始化 HttpClient
    m_pHttpClient = new HttpClient(this);

    m_videoWidget = ui->webView;
    libvlc_media_player_set_hwnd(m_vlc_mediaPlayer, (void *)m_videoWidget->winId());

    // 连接 TreeWidget 的项点击事件
    connect(ui->treeWidget, &QTreeWidget::itemDoubleClicked, this, &MediayPlayerWidget::onItemDoubleClicked);
    connect(ui->refreshBtn, &QPushButton::clicked, this, &MediayPlayerWidget::onRefreshBtnClicked);

    connect(m_pHttpClient, &HttpClient::parserFinished, this, &MediayPlayerWidget::onParserFinished);

    onRefreshBtnClicked(); // 先刷新一次
}

void MediayPlayerWidget::play(const QString & url)
{
    if (media)
    {
        libvlc_media_release(media);
    }

    // 创建媒体对象
    media = libvlc_media_new_location(m_vlcInstance, url.toStdString().c_str());
    libvlc_media_player_set_media(m_vlc_mediaPlayer, media);

    // 开始播放
    libvlc_media_player_play(m_vlc_mediaPlayer);
}

MediayPlayerWidget::~MediayPlayerWidget()
{
    delete ui;
}

void MediayPlayerWidget::updateTreeWidget()
{
    m_pHttpClient->updateTreeWidget(ui->treeWidget);
}

void MediayPlayerWidget::onRefreshBtnClicked()
{
    // 请求一次 然后刷新
    m_pHttpClient->getFileList();
}

void MediayPlayerWidget::onItemDoubleClicked(QTreeWidgetItem * item, int column)
{
    FileInfo fileInfo = item->data(0, Qt::UserRole).value<FileInfo>();

    if (fileInfo.isDir == false)
    {
         play(m_pHttpClient->getVedioUrl(fileInfo.fullPath));
    }
}

void MediayPlayerWidget::onParserFinished()
{
    updateTreeWidget();
}
