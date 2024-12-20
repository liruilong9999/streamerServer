#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QTreeWidgetItem>
#include <QVariant>

struct FileInfo
{
    QString         name;
    bool            isDir;
    QList<FileInfo> children; // 子目录列表（如果是目录的话）
    QString         fullPath; // 记录文件或目录的完整路径
};
Q_DECLARE_METATYPE(FileInfo)

class HttpClient : public QObject
{
    Q_OBJECT

public:
    HttpClient(QObject * parent = nullptr);
    ~HttpClient();

    // 获取文件列表并更新到 TreeWidget
    void updateTreeWidget(QTreeWidget * treeWidget);

    void getFileList();
    QString getVedioUrl(QString path);

signals:
    void parserFinished();

private slots:
    void onRequestFinished();

private:
    void loadConfig();
    void parseFileList(const QJsonArray & fileArray, QList<FileInfo> & fileList, const QString & parentDir = "");
    void parseFileListRecursively(const QJsonArray & childrenArray, QList<FileInfo> & childrenList, const QString & parentDir);
    void addSubdirectories(QTreeWidgetItem * parentItem, const FileInfo & parentInfo);

private:
    QNetworkAccessManager * m_pManager{nullptr};
    QNetworkReply *         m_pReply{nullptr};
    QString                 m_ip;
    int                     m_port;
    QList<FileInfo>         m_fileList;
};

#endif // HTTPCLIENT_H
