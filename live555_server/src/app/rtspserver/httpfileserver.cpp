#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>
#include <QSettings>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include "httpfileserver.h"

// 构造函数
HttpFileServer::HttpFileServer(QObject * parent)
    : QTcpServer(parent)
    , m_port(8082) // 默认端口设置为 8080
{
    loadSettings(); // 从配置文件加载设置
}

// 析构函数
HttpFileServer::~HttpFileServer() = default;

// 从配置文件加载服务器设置（端口和目录）
void HttpFileServer::loadSettings()
{
    QSettings settings("config/config.ini", QSettings::IniFormat);

    m_directory = settings.value("Server/directory", "/path/to/default/video/files").toString();
    m_port      = settings.value("Server/port", 8080).toInt();

    qDebug() << "Server directory: " << m_directory;
    qDebug() << "Server port: " << m_port;

    // 创建输出目录
    // 定义目标路径
    QString path = m_directory + "/outdir";

    // 创建 QDir 对象
    QDir dir;

    // 检查路径是否存在
    if (!dir.exists(path))
    {
        // 如果路径不存在，尝试创建
        if (dir.mkpath(path))
        {
            qDebug() << "目录创建成功:" << path;
        }
        else
        {
            qDebug() << "目录创建失败:" << path;
        }
    }
    else
    {
        qDebug() << "目录已存在:" << path;
    }
}

// 启动服务器，使用从配置文件加载的端口
void HttpFileServer::startServer()
{
    if (this->listen(QHostAddress::Any, m_port))
    {
        qDebug() << "Server started on port" << m_port;
    }
    else
    {
        qDebug() << "Server failed to start: " << this->errorString();
    }
}

// 处理传入的TCP连接
void HttpFileServer::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket * socket = new QTcpSocket(this);
    if (!socket->setSocketDescriptor(socketDescriptor))
    {
        delete socket;
        return;
    }

    connect(socket, &QTcpSocket::readyRead, [this, socket]() {
        QByteArray  requestData  = socket->readAll();
        QString     request      = QString::fromUtf8(requestData);
        QStringList requestLines = request.split("\r\n");
        if (requestLines.isEmpty())
        {
            socket->close();
            return;
        }

        QString     requestLine  = requestLines.first();
        QStringList requestParts = requestLine.split(" ");
        if (requestParts.size() < 2)
        {
            socket->close();
            return;
        }

        QString requestedFile = requestParts.at(1);

        if (requestedFile == "/filelist")
        {
            sendFileList(socket); // 返回文件列表
        }
    });

    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

// 处理文件列表请求并返回 JSON 格式的文件结构
void HttpFileServer::sendFileList(QTcpSocket * socket)
{
    QJsonArray fileList = getFileListFromDir(m_directory); // 获取文件列表

    // 构建 JSON 响应
    QJsonObject jsonResponse;
    jsonResponse["files"] = fileList;

    // 转换 JSON 对象为字节数组
    QJsonDocument jsonDoc(jsonResponse);               // 使用 QJsonDocument 来封装 QJsonObject
    QByteArray    jsonResponseData = jsonDoc.toJson(); // 获取 JSON 字符串

    // 生成 HTTP 头
    QString httpHeader = QString(
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: application/json\r\n"
                             "Content-Length: %1\r\n"
                             "Connection: close\r\n"
                             "Date: %2\r\n\r\n")
                             .arg(jsonResponseData.size())
                             .arg(QDateTime::currentDateTime().toString("ddd, dd MMM yyyy hh:mm:ss GMT"));

    // 发送 HTTP 响应头和文件列表数据
    socket->write(httpHeader.toUtf8());
    socket->write(jsonResponseData);
    socket->flush();
    socket->close();
}

// 递归获取文件和目录的 JSON 列表
QJsonArray HttpFileServer::getFileListFromDir(const QString & dirPath)
{
    QDir       dir(dirPath);
    QJsonArray jsonArray;

    if (!dir.exists())
    {
        return jsonArray; // 如果目录不存在，返回空的 JSON 数组
    }

    // 获取目录下的文件和子目录
    QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    foreach (const QFileInfo & fileInfo, fileInfoList)
    {
        QJsonObject fileObject;
        fileObject["name"]  = fileInfo.fileName();
        fileObject["isDir"] = fileInfo.isDir();

        if (fileInfo.isDir())
        {
            // 如果是目录，递归调用获取文件夹内容
            QJsonArray childrenArray = getFileListFromDir(fileInfo.absoluteFilePath());
            fileObject["children"]   = childrenArray; // 递归填充子目录的内容
        }

        jsonArray.append(fileObject); // 将文件或目录添加到 JSON 数组中
    }

    return jsonArray; // 返回目录及其子文件的 JSON 数组
}
