/*!
 * \file  .\src\lib\videocommon\playerwindow.h.
 *
 * Declares the playerwindow class.
 */

#ifndef PLAYERWINDOW_H
#define PLAYERWINDOW_H

#include <QContextMenuEvent>
#include <QTimer>
#include <QWidget>

#include "playerthread.h"
#include "videocommon_global.h"

namespace Ui {
class PlayerWindow;
}

struct PlayerWindowPrivate;

class VIDEOCOMMON_EXPORT PlayerWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PlayerWindow(bool isUseRightMouse = false, QWidget * parent = nullptr);
    ~PlayerWindow() override;

    void openUrl(QString url);

protected:
    void contextMenuEvent(QContextMenuEvent * event) override;
    void resizeEvent(QResizeEvent * event) override;

private slots:
    void onTimerUpdate();

private:
    Ui::PlayerWindow *    ui{nullptr};
    PlayerWindowPrivate * m_pPrivate{nullptr};

    PlayerThread * m_pPlayerThread{nullptr};
    QTimer *       m_pFrameTimer{nullptr};

    int m_videoWidth{0};
    int m_videoHeight{0};

    CircularQueue<AVFrame *> m_frameQueue{5};

    bool m_isUseRightMouse{false};
    int  m_duration{0};
    int  m_fps{30};
};

#endif // PLAYERWINDOW_H
