#ifndef MEDIAYPLAYERWIDGET_H
#define MEDIAYPLAYERWIDGET_H

#include <QWidget>
#include <vlc/vlc.h>
#include <QTimer>
#include <QProcess>
#include <QSlider>

#include "httpclient.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MediayPlayerWidget;
}
QT_END_NAMESPACE

class MediayPlayerWidget : public QWidget
{
    Q_OBJECT

public:
    MediayPlayerWidget(QWidget * parent = nullptr);
	void play(const QString & url);
	~MediayPlayerWidget();

    void updateTreeWidget();

public slots:
    // 获取文件列表
    void onRefreshBtnClicked();
    void onItemDoubleClicked(QTreeWidgetItem * item, int column);
    void onParserFinished();

private:
    Ui::MediayPlayerWidget * ui;

    HttpClient * m_pHttpClient{nullptr};
    QWidget *    m_videoWidget{nullptr};    // 播放窗口

    libvlc_media_player_t * m_vlc_mediaPlayer{nullptr}; // 播放控制

    QTimer * m_timer{nullptr};
    libvlc_instance_t * m_vlcInstance{nullptr}; // 连接vlc库的全局配置
    libvlc_media_t *    m_vlc_media{nullptr};   // 文件
    libvlc_media_t *    media{nullptr}; 
};
#endif // MEDIAYPLAYERWIDGET_H
