#include <QApplication>
#include <LWebView/LWebView.h>

#include "mediayplayerwidget.h"

int main(int argc, char * argv[])
{
    QApplication a(argc, argv);
	LWebView::Init();
    // 创建客户端实例
    MediayPlayerWidget w;
	w.show();

    return a.exec();
}
