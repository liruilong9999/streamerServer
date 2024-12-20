#include "LWebView.h"

#include <QContextMenuEvent>
#include <QFile>
#include <QFileDialog>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkProxyFactory>
#include <QTextStream>
#include <QWebChannel>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QWebEngineScriptCollection>

namespace {
inline QString LoadText(const QString & path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly))
    {
        QTextStream ts(&file);
        ts.setCodec("UTF-8");
        return ts.readAll();
    }
    return QString();
}

class LWebViewInitializer
{
public:
    LWebViewInitializer()
    {
        // 在页面加载时注入自定义 Javascript 脚本 qwebchannel.js
        QWebEngineScript script;
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setWorldId(QWebEngineScript::MainWorld);

		// script.setSourceCode(LoadText(":/file/qwebchannel.js"));  如果这里崩溃，就需要将qtcore动态库替换，
		// 因为通过qt安装器下载的动态库是阉割版，将源码重新编译再将动态库放在执行文件目录下即可
        script.setSourceCode(LoadText(":/file/qwebchannel.js")); 
        if ((_Ok = !script.isNull()))
        {
            QWebEngineProfile*aa = QWebEngineProfile::defaultProfile();
			QWebEngineScriptCollection *bb = aa->scripts();
            QWebEngineProfile::defaultProfile()->scripts()->insert(script);
        }

        QNetworkProxyFactory::setUseSystemConfiguration(false);
    }

public:
    bool isOk() const { return _Ok; }

private:
    bool _Ok;
};

} // namespace

struct LWebViewPrivate
{
    LWebViewPrivate()
        : _loadProgress(0)
    {}

    int _loadProgress;
};

LWebView::LWebView(QWidget * parent /*= nullptr*/)
    : QWebEngineView(parent)
    , _p(new LWebViewPrivate)
{
    connect(this, &LWebView::iconChanged, [this](const QIcon & icon) {
        setWindowIcon(icon);
    });
    connect(this, &LWebView::loadStarted, [this]() {
        _p->_loadProgress = 0;
    });
    connect(this, &LWebView::loadProgress, [this](int progress) {
        _p->_loadProgress = progress;
    });
    connect(this, &LWebView::loadFinished, [this](bool ok) {
        if (!ok)
            _p->_loadProgress = 0;
    });
    connect(this, &LWebView::renderProcessTerminated, 
		[this](QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode) {
        QString status;
        switch (terminationStatus)
        {
        case QWebEnginePage::AbnormalTerminationStatus :
            // status = FromAscii("渲染进程非正常退出");
            break;
        case QWebEnginePage::CrashedTerminationStatus :
            // status = FromAscii("渲染进程崩溃");
            break;
        case QWebEnginePage::KilledTerminationStatus :
            // status = FromAscii("渲染进程被外部终止");
            break;
        default :
            // status = FromAscii("渲染进程正常退出");
            break;
        }
        if (QMessageBox::question(window(), tr("Error: %1").arg(exitCode), 
			tr("%1\r\n Is restore the page？").arg(status), tr("yes"), tr("no")) == 0)
        {
            QMetaObject::invokeMethod(this, "reload", Qt::QueuedConnection);
        }
    });
    connect(this, &LWebView::titleChanged, [this](const QString & title) {
        setWindowTitle(title);
    });
    setPage(new LWebBrowser(QWebEngineProfile::defaultProfile(), this));

    connect(QWebEngineProfile::defaultProfile(), SIGNAL(downloadRequested(QWebEngineDownloadItem *)), 
		this, SLOT(OnDownloadRequestedSlot(QWebEngineDownloadItem *)));
}

LWebView::~LWebView()
{
    delete _p;
}

LWebBrowser * LWebView::Browser() const
{
    return static_cast<LWebBrowser *>(page());
}

bool LWebView::Init()
{
    static LWebViewInitializer Initializer;
    return Initializer.isOk();
    return true;
}

int LWebView::LoadProgress() const
{
    return _p->_loadProgress;
}

void LWebView::RegisterObject(const QString & name, QObject * obj)
{
    page()->webChannel()->registerObject(name, obj);
}

void LWebView::UnregisterObject(QObject * obj)
{
    page()->webChannel()->deregisterObject(obj);
}

bool LWebView::IsRegisterObject(QString ObjName)
{
    QHash<QString, QObject *> str = page()->webChannel()->registeredObjects();
    if (page()->webChannel()->registeredObjects().contains(ObjName))
        return true;
    return false;
}

void LWebView::contextMenuEvent(QContextMenuEvent * e)
{
	return;
    // 只显示重新载入命令
    QMenu menu;
    menu.addAction(tr("refresh"), this, SLOT(reload()));
    menu.addAction(tr("debug"), this, SLOT(ShowDevToolsPageSlot()));
    menu.addAction(tr("clearCache"), this, SLOT(ClearCacheSlot()));
    menu.exec(e->globalPos());
}

QWebEngineView * LWebView::createWindow(QWebEnginePage::WebWindowType type)
{
    Q_UNUSED(type)
    return nullptr;
}

bool DeleteDir(const QString & Path)
{
    if (Path.isEmpty())
    {
        return false;
    }

    QDir Dir(Path);
    if (!Dir.exists())
    {
        return true;
    }

    Dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);

    QFileInfoList FileList = Dir.entryInfoList();
    for (QFileInfo File : FileList)
    {
        if (File.isFile())
        {
            File.dir().remove(File.fileName());
        }
        else
        {
            DeleteDir(File.absoluteFilePath());
        }
    }

    return Dir.rmpath(Dir.absolutePath());
}

void LWebView::ShowDevToolsPageSlot()
{
    if (QWebEnginePage * CurPage = page())
    {
        QMainWindow * NewMainWindow = new QMainWindow(this);

        QWebEngineView * NewWebView = new QWebEngineView(NewMainWindow);

        NewMainWindow->setCentralWidget(NewWebView);

        CurPage->setDevToolsPage(NewWebView->page());
        CurPage->triggerAction(QWebEnginePage::InspectElement);
        NewMainWindow->show();
    }
}

void LWebView::ClearCacheSlot()
{
    if (!page())
    {
        return;
    }

    if (QWebEngineProfile * EngineProfile = page()->profile())
    {
        EngineProfile->clearHttpCache();
        EngineProfile->clearAllVisitedLinks();

        if (QWebEngineCookieStore * CookieStore = page()->profile()->cookieStore())
        {
            CookieStore->deleteAllCookies();
            CookieStore->deleteSessionCookies();
        }

        QString CachePath = EngineProfile->cachePath();
    }
}

void LWebView::OnDownloadRequestedSlot(QWebEngineDownloadItem * download)
{
    if (download->state() == QWebEngineDownloadItem::DownloadRequested)
    {
        QString Path = QFileDialog::getSaveFileName(nullptr, tr("save as"), download->path(), 
			tr("all files(*.*)"), 0, QFileDialog::DontUseNativeDialog);

        if (Path.isEmpty())
        {
            download->cancel();
        }
        if (!Path.isEmpty())
        {
            download->setPath(Path);
            download->accept();
        }
    }
}
