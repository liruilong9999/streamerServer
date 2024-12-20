#include "httpfileserver.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>
#include <QSettings>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

// 构造函数
HttpFileServer::HttpFileServer(QObject * parent)
    : QTcpServer(parent)
    , m_port(8080) // 默认端口设置为 8080
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

        if (requestedFile == "/")
        {
            requestedFile = "/index.html"; // 默认页面
        }

        if (requestedFile == "/filelist")
        {
            sendFileList(socket); // 返回文件列表
        }
        else
        {
            QString   filePath = m_directory + requestedFile;
            QFileInfo fileInfo(filePath);

            if (fileInfo.exists() && fileInfo.isFile())
            {
                sendHttpResponse(socket, filePath, requestData);
            }
            else
            {
                socket->write("HTTP/1.1 404 Not Found\r\n\r\nFile Not Found");
                socket->close();
            }
        }
    });

    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

// 发送 HTTP 响应和文件内容
void HttpFileServer::sendHttpResponse(QTcpSocket * socket, const QString & filePath, QByteArray requestData)
{
    QFile file(filePath); // 创建文件对象
    if (!file.open(QIODevice::ReadOnly))
    {
        socket->write("HTTP/1.1 500 Internal Server Error\r\n\r\nUnable to read file");
        socket->close();
        return;
    }

    QFileInfo fileInfo(filePath);
    QString   fileName = fileInfo.fileName();
    qint64    fileSize = fileInfo.size();

    // 获取请求的 Range（即用户拖动进度条时会发送的范围）
    qDebug() << requestData;

    QString request = QString::fromUtf8(requestData);

	QStringList requestLines = request.split("\r\n");
    QString     rangeHeader;
    for (const QString & line : requestLines)
    {
        if (line.startsWith("Range:"))
        {
            rangeHeader = line;
            break;
        }
    }

    qint64 startByte = 0;
    qint64 endByte   = fileSize - 1;

    // 检查请求头是否包含 "Range" 字段
    if (rangeHeader.contains("Range"))
    {
        // 解析 Range 请求
        QRegExp rangeRegex("Range: bytes=(\\d*)-(\\d*)");
        // QRegExp rangeRegex("Range: bytes=(\\d+)-(\\d+)");
        int pos = rangeRegex.indexIn(request);
        if (pos != -1)
        {
            startByte = rangeRegex.cap(1).toLongLong();
            endByte   = rangeRegex.cap(2).toLongLong();
        }
    }
    if (endByte == 0)
    {
        if (15 * 1024 * 1024+startByte < fileSize - 1) // 文件小于15M的话，直接缓存整个文件，否则就15M
        {
            endByte = 15 * 1024 * 1024+startByte;
        }
		else
        {
            endByte = fileSize - 1;
		}
	}
	else
	{
        if (15 * 1024 * 1024 < fileSize - 1) // 文件小于15M的话，直接缓存整个文件，否则就15M
        {
            endByte = 15 * 1024 * 1024+startByte;
        }
        else
        {
            endByte = fileSize - 1;
        }
	}


    // 确保返回的字节范围合法
    if (startByte < 0)
        startByte = 0;
    if (endByte >= fileSize)
        endByte = fileSize - 1;

    // 定位到请求的字节位置
    file.seek(startByte);

    QByteArray fileData = file.read(endByte - startByte + 1);

    // 构建 HTTP 头，支持 Range 请求
    QString httpHeader = QString(
                             "HTTP/1.1 206 Partial Content\r\n"
                             "Content-Type: video/mp4\r\n"
                             "Content-Length: %1\r\n"
                             "Content-Range: bytes %2-%3/%4\r\n"
                             "Connection: close\r\n"
                             "Content-Disposition: inline; filename=%5\r\n"
                             "Date: %6\r\n\r\n")
                             .arg(fileData.size())
                             .arg(startByte)
                             .arg(endByte)
                             .arg(fileSize)
                             .arg(fileName)
                             .arg(QDateTime::currentDateTime().toString("ddd, dd MMM yyyy hh:mm:ss GMT"));

	qDebug()<<httpHeader;
    // 发送响应头
    socket->write(httpHeader.toUtf8());
    // 发送文件数据
    socket->write(fileData);
    socket->flush();
    socket->close();
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