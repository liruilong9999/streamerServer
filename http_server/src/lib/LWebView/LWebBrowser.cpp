
#include "LWebBrowser.h"
#include <QFileDialog>
#include <QListView>
#include <QTreeView>
#include <QFile>
#include <QWebChannel>
#include <QWebEngineCertificateError>
#include <QWebEngineScript>
#include <QMessageBox>

LWebBrowser::LWebBrowser(QWebEngineProfile* profile, QWidget* parent /*= nullptr*/)
 : QWebEnginePage(profile, parent)
{
    // 建立与 Javascript 的通信服务
    setWebChannel(new QWebChannel(this), QWebEngineScript::MainWorld);
}

bool LWebBrowser::certificateError(const QWebEngineCertificateError& error)
{
	QWidget * window = view()->window();
    if (error.isOverridable())
    {
        // 选择第1个按钮时继续
        return QMessageBox::warning(window, tr("Certificate error"), error.errorDescription() + tr("\r\n Do you want to proceed with the request？"), tr("yes"), tr("no")) == 0;
		return true;
	}
    else
    {
        QMessageBox::warning(window, tr("Certificate error"), error.errorDescription(), tr("yes"));
        return false;
    }
}

void LWebBrowser::javaScriptAlert(const QUrl& securityOrigin, const QString& msg)
{
	Q_UNUSED(securityOrigin)
	QMessageBox::information(view()->window(), tr("Javascript Alert"), msg, tr("ok"));
}

bool LWebBrowser::javaScriptConfirm(const QUrl& securityOrigin, const QString& msg)
{
	Q_UNUSED(securityOrigin)
	return QMessageBox::information(view()->window(), tr("Javascript Confirm"), msg, tr("ok"), tr("cancel")) == 0;
}

bool LWebBrowser::javaScriptPrompt(const QUrl& securityOrigin, const QString& msg, const QString& defaultValue, QString* result)
{
	return true;
}