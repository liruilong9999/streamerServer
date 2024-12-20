#ifndef LWEBBROWSER_H
#define LWEBBROWSER_H

#include <QWebEnginePage>

#include "LWebView_Global.h"


class LWEBVIEW_EXPORT LWebBrowser : public QWebEnginePage
{
public:
    explicit LWebBrowser(QWebEngineProfile* profile, QWidget* parent = nullptr);

protected:
    // 证书错误处理 
    bool certificateError(const QWebEngineCertificateError& error);
    void javaScriptAlert(const QUrl& securityOrigin, const QString& msg);
    bool javaScriptConfirm(const QUrl& securityOrigin, const QString& msg);
    bool javaScriptPrompt(const QUrl& securityOrigin, const QString& msg, const QString& defaultValue, QString* result);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>   重写上传文件方法,自定义打开模式,不用操作系统打开,改用Qt打开. </summary>
    ///
    /// <remarks>   李瑞龙, 2023/3/1 星期三. </remarks>
    ///
    /// <param name="mode">                 The mode. </param>
    /// <param name="oldFiles">             The old files. </param>
    /// <param name="acceptedMimeTypes">    List of types of the accepted mimes. </param>
    ///
    /// <returns>   A QStringList. </returns>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    //QStringList chooseFiles(QWebEnginePage::FileSelectionMode mode, const QStringList& oldFiles, const QStringList& acceptedMimeTypes);

signals:
    void fileUploaded(const QByteArray& data);
};

#endif // LWEBBROWSER_H
