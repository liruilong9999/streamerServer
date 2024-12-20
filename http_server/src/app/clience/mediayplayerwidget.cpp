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

    // 初始化 HttpClient
    m_pHttpClient = new HttpClient(this);

    // 连接 TreeWidget 的项点击事件
    connect(ui->treeWidget, &QTreeWidget::itemDoubleClicked, this, &MediayPlayerWidget::onItemDoubleClicked);
    connect(ui->refreshBtn, &QPushButton::clicked, this, &MediayPlayerWidget::onRefreshBtnClicked);
    connect(ui->webView, &LWebView::loadProgress, this, &MediayPlayerWidget::onLoadProgress);
    connect(ui->webView, &LWebView::titleChanged, this, &MediayPlayerWidget::onTitleChanged);
    connect(ui->webView, &LWebView::urlChanged, this, &MediayPlayerWidget::onUrlChanged);

    connect(m_pHttpClient, &HttpClient::parserFinished, this, &MediayPlayerWidget::onParserFinished);

    onLoadProgress(ui->webView->LoadProgress());

    onRefreshBtnClicked(); // 先刷新一次
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
        ui->webView->setUrl(m_pHttpClient->getVedioUrl(fileInfo.fullPath));
    }
}

void MediayPlayerWidget::onLoadProgress(int progress)
{
    // 显示加载进度
    progress = progress < 100 ? progress : 0;
    ui->progressBar->setValue(progress);
    ui->progressBar->setVisible(progress > 0);
}

void MediayPlayerWidget::onTitleChanged(const QString & title)
{
    // setWindowTitle(title);
}

void MediayPlayerWidget::onUrlChanged(const QUrl & url)
{
}

void MediayPlayerWidget::onParserFinished()
{
    updateTreeWidget();
}
