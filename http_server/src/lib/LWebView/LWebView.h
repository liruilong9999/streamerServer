
#ifndef LWEBVIEW_H__
#define LWEBVIEW_H__

#include <QObject>
#include <QWebChannel>
#include <QWebEngineView>
#include <QWebEnginePage>
#include "LWebBrowser.h"

class LWebBrowser;
struct LWebViewPrivate;

class LWEBVIEW_EXPORT LWebView : public QWebEngineView
{
    Q_OBJECT

public:
    explicit LWebView(QWidget * parent = nullptr);
    ~LWebView();

public:
    // 返回浏览器页面控制接口, 可通过 runJavaScript 接口调用 Javascript.
    LWebBrowser * Browser() const;
    // 在创建任一 HRLJWebView 前调用此方法初始化.
    static bool Init();

    // 获取页面加载进度, 范围0-100.
    int LoadProgress() const;

    // 将 obj 使用 name 作为名称注册到 Javascript 供全局访问.
    // name:名称, 建议不要和常用全局变量名称(如 window)相同
    // obj: the object.
    void RegisterObject(const QString & name, QObject * obj);
    void UnregisterObject(QObject * obj);

    bool IsRegisterObject(QString ObjName);

protected:
    // 上下文菜单.
    void contextMenuEvent(QContextMenuEvent * e);
    //////////////////////////////////////////////////////////////////////////
    /// <summary> 使用 Javascript 创建新窗口/标签页时实现此函数. </summary>
    /// <param name="type"> 窗口类型. </param>
    /// <returns> 新窗口/标签页对应的 QWebEngineView 指针. </returns>
    //////////////////////////////////////////////////////////////////////////
    QWebEngineView * createWindow(QWebEnginePage::WebWindowType type);

protected slots:
    void ShowDevToolsPageSlot();
    void ClearCacheSlot();
    void OnDownloadRequestedSlot(QWebEngineDownloadItem * download);

private:
    using QWebEngineView::page;
    using QWebEngineView::setPage;

    LWebViewPrivate * _p;
};

#endif
