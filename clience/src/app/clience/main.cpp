#include <QApplication>

#include "mediayplayerwidget.h"

int main(int argc, char * argv[])
{
    QApplication a(argc, argv);
    // 创建客户端实例
    MediayPlayerWidget w;
	w.showMaximized();

    return a.exec();
}
