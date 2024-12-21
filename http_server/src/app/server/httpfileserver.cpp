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
#include <QUrlQuery>

#define CACTH_SIZE 20 * 1024 * 1024

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
QString HttpFileServer::getRequestedFile(const QString & requestStr)
{
    // 假设请求格式为 GET /filelist?path=dir1 HTTP/1.1
    int startIdx = requestStr.indexOf("GET ");
    int endIdx   = requestStr.indexOf(" HTTP");

    if (startIdx != -1 && endIdx != -1)
    {
        QString requestedFile = requestStr.mid(startIdx + 4, endIdx - startIdx - 4); // 提取路径部分

        // 去除查询字符串部分
        int queryIndex = requestedFile.indexOf('?');
        if (queryIndex != -1)
        {
            requestedFile = requestedFile.left(queryIndex); // 去除查询参数
        }

        return requestedFile;
    }

    return QString();
}

// 获取查询参数
QString HttpFileServer::getQueryParameter(const QString & requestStr, const QString & param)
{
    // 获取请求行中的 URL 部分
    int startIdx = requestStr.indexOf("GET ") + 4; // 跳过 "GET "
    int endIdx   = requestStr.indexOf(" HTTP");

    if (startIdx != -1 && endIdx != -1)
    {
        QString url = requestStr.mid(startIdx, endIdx - startIdx); // 获取路径部分（包括查询字符串）

        // 查找查询字符串的开始部分 '?'
        int queryStart = url.indexOf('?');
        if (queryStart != -1)
        {
            // 获取查询字符串部分
            QString query = url.mid(queryStart + 1); // 只保留查询参数

            // 使用 QUrlQuery 解析查询参数
            QUrlQuery urlQuery(query);
            return urlQuery.queryItemValue(param); // 获取指定的参数值（例如 'path'）
        }
    }
    return QString(); // 如果没有查询字符串或参数，返回空
}

void HttpFileServer::sendNotFound(QTcpSocket * socket)
{
    QByteArray notFoundResponse = "HTTP/1.1 404 Not Found\r\n\r\nFile Not Found";
    socket->write(notFoundResponse);
    socket->close();
}

void HttpFileServer::sendHtmlFile(QTcpSocket * socket, QString htmlPath)
{
    QFile file(htmlPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "Could not open video.html";
        sendNotFound(socket);
        return;
    }
    QByteArray fileData = file.readAll();
    QString    response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: " +
        QString::number(fileData.size()) +
        "\r\n"
        "Connection: close\r\n"
        "Date: " +
        QDateTime::currentDateTime().toString("ddd, dd MMM yyyy hh:mm:ss GMT") + "\r\n\r\n";

    socket->write(response.toUtf8());
    socket->write(fileData);
    socket->flush();
    socket->close();
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
        QByteArray requestData = socket->readAll();              // 读取请求数据
        QString    request     = QString::fromUtf8(requestData); // 转换为字符串

        // 按行分割请求
        QStringList requestLines = request.split("\r\n");
        if (requestLines.isEmpty())
        {
            socket->close();
            return;
        }

        QString     requestLine  = requestLines.first();   // 获取请求行
        QStringList requestParts = requestLine.split(" "); // 按空格分割请求行
        if (requestParts.size() < 2)
        {
            socket->close();
            return;
        }

        QString requestedFile = requestParts.at(1); // 获取请求的路径

        // 去除查询参数（如果有）
        int queryIndex = requestedFile.indexOf('?');
        if (queryIndex != -1)
        {
            requestedFile = requestedFile.left(queryIndex); // 只保留路径部分
        }

        // qDebug() << "Requested File: " << requestedFile; // 打印请求的路径，调试用

        // 获取请求的查询参数（例如：?path=dir1）
        QString path = getQueryParameter(request, "path"); // 从请求中提取查询参数
                                                           // qDebug() << "Requested Path: " << path;            // 打印路径，调试用

        if (requestedFile == "/")
        {
            requestedFile = "./html/video.html"; // 默认页面
            sendHtmlFile(socket, requestedFile);
            return;
        }
        // 根据请求的路径判断
        if (requestedFile == "/filelist")
        {
            sendFileList(socket, path); // 传递路径参数
        }
        else
        {
            QString   filePath = m_directory + requestedFile;
            QFileInfo fileInfo(filePath);

            if (fileInfo.exists() && fileInfo.isFile())
            {
                sendHttpResponse(socket, filePath, requestData); // 发送文件内容
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
        if (CACTH_SIZE + startByte < fileSize - 1) // 文件小于15M的话，直接缓存整个文件，否则就15M
        {
            endByte = CACTH_SIZE + startByte;
        }
        else
        {
            endByte = fileSize - 1;
        }
    }
    else
    {
        if (CACTH_SIZE < fileSize - 1) // 文件小于15M的话，直接缓存整个文件，否则就15M
        {
            endByte = CACTH_SIZE + startByte;
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
    // 发送响应头
    socket->write(httpHeader.toUtf8());
    // 发送文件数据
    socket->write(fileData);
    socket->flush();
    socket->close();
}

// 处理文件列表请求并返回 JSON 格式的文件结构
void HttpFileServer::sendFileList(QTcpSocket * socket, const QString & path)
{
    // qDebug() << "Received path: " << path; // 输出路径，用于调试

    // 使用 path 来获取文件列表，假设您有一个根据路径获取文件夹内容的函数
    QJsonArray fileList = getFileListFromDir(path.isEmpty() ? m_directory : m_directory + "/" + path); // 如果没有 path 参数，使用默认目录

    // qDebug() << "File list: " << fileList; // 输出获取到的文件列表，用于调试

    // 构建 JSON 响应
    QJsonObject jsonResponse;
    jsonResponse["files"] = fileList;

    // 转换 JSON 对象为字节数组
    QJsonDocument jsonDoc(jsonResponse);               // 使用 QJsonDocument 来封装 QJsonObject
    QByteArray    jsonResponseData = jsonDoc.toJson(); // 获取 JSON 字符串

    // qDebug() << "JSON Response: " << jsonResponseData; // 输出响应内容，用于调试

    // 生成 HTTP 响应头
    QString httpHeader = QString(
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: application/json\r\n"
                             "Content-Length: %1\r\n"
                             "Access-Control-Allow-Origin: *\r\n" // 允许跨域
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