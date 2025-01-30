#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <regex>
#include <string>
#include <sstream>

#include <ByteStreamFileSource.hh>
#include <H264VideoStreamFramer.hh>
#include <H264VideoRTPSink.hh>
#include <H264VideoFileServerMediaSubsession.hh>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
}

#include "seekh264videofileservermediasubsession.h"

#ifdef _WIN32
#define popen  _popen
#define pclose _pclose
#endif

// 获取文件大小
uint64_t getFileSize(const std::string & fileName)
{
    struct stat stat_buf;
    int         rc = stat(fileName.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

// 计算比特率(针对264文件)
int calculateBitrate(const std::string & fileName, double duration)
{
    uint64_t fileSize = getFileSize(fileName);
    if (fileSize == -1)
    {
        std::cerr << "Failed to get file size" << std::endl;
        return -1;
    }

    // 比特率 (bits per second) = (文件大小 (bytes) * 8) / 时长 (seconds)
    int bitrate = static_cast<int>((fileSize * 8) / duration);
    return bitrate;
}

bool getVideoDuration(const char * inputFilename, double & durationInSeconds)
{
    AVFormatContext * formatContext = nullptr;
    int               ret           = avformat_open_input(&formatContext, inputFilename, nullptr, nullptr);
    if (ret < 0)
    {
        std::cerr << "Could not open input file " << inputFilename << std::endl;
        return false;
    }

    // 获取流信息
    ret = avformat_find_stream_info(formatContext, nullptr);
    if (ret < 0)
    {
        std::cerr << "Could not find stream information" << std::endl;
        avformat_close_input(&formatContext);
        return false;
    }

    // 打印流信息
    // av_dump_format(formatContext, 0, inputFilename, 0);

    // 获取视频流
    int videoStreamIndex = -1;
    for (unsigned i = 0; i < formatContext->nb_streams; i++)
    {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1)
    {
        std::cerr << "Could not find video stream" << std::endl;
        avformat_close_input(&formatContext);
        return false;
    }

    // 获取视频流的时长
    AVStream * videoStream = formatContext->streams[videoStreamIndex];
    // 使用 AVStream 的 duration 字段（单位：时间基准）
    int64_t    duration = videoStream->duration; // in time base units
    AVRational timeBase = videoStream->time_base;

    // 将时长转换为秒
    durationInSeconds = static_cast<double>(duration) * av_q2d(timeBase);

    // 释放资源
    avformat_close_input(&formatContext);

    return true;
}

SeekH264VideoFileServerMediaSubsession::SeekH264VideoFileServerMediaSubsession(
    UsageEnvironment & env, char const * outFileName, char const * inFileName, Boolean reuseFirstSource)
    : H264VideoFileServerMediaSubsession(env, outFileName, reuseFirstSource)
    , m_fDurationSeconds(0.0)
    , m_getBitrate(0)
{
    // 通过 ffprobe 或其他方法计算视频时长

    double durationSeconds = 0.0;
    getVideoDuration(inFileName, durationSeconds);
    m_fDurationSeconds = durationSeconds;
    m_getBitrate       = calculateBitrate(outFileName, m_fDurationSeconds);
    // m_getBitrate       = getBitrate(inFileName);
}

SeekH264VideoFileServerMediaSubsession::~SeekH264VideoFileServerMediaSubsession()
{}

SeekH264VideoFileServerMediaSubsession * SeekH264VideoFileServerMediaSubsession::createNew(
    UsageEnvironment & env, char const * outFileName, char const * inFileName, Boolean reuseFirstSource)
{
    return new SeekH264VideoFileServerMediaSubsession(env, outFileName, inFileName, reuseFirstSource);
}

float SeekH264VideoFileServerMediaSubsession::duration() const
{
    return m_fDurationSeconds;
}

void SeekH264VideoFileServerMediaSubsession::seekStreamSource(
    FramedSource * inputSource, double & seekNPT, double streamDuration, u_int64_t & numBytes)
{
    // 计算字节速率
    unsigned byteRate = m_getBitrate / 8; // 转换为 bytes per second

    // 计算字节偏移量
    u_int64_t seekByteOffset = static_cast<u_int64_t>(seekNPT * byteRate);

    H264VideoStreamFramer * framer = dynamic_cast<H264VideoStreamFramer *>(inputSource);
    if (framer == nullptr)
    {
        std::cerr << "Error: inputSource is not a H264VideoStreamFramer" << std::endl;
        return;
    }

    // 获取底层的 ByteStreamFileSource
    FramedSource * underlyingSource = framer->inputSource();
    if (underlyingSource == nullptr)
    {
        std::cerr << "Error: underlyingSource is null" << std::endl;
        return;
    }

    ByteStreamFileSource * byteSource = dynamic_cast<ByteStreamFileSource *>(underlyingSource);
    // 通知输入源新的字节偏移量
    if (byteSource)
    {
        byteSource->seekToByteAbsolute(seekByteOffset);
        // 更新剩余字节数
        numBytes = byteSource->fileSize() - seekByteOffset;
        // std::cout << "numBytes:" << numBytes << std::endl;
        // std::cout << "byteSource->fileSize():" << byteSource->fileSize() << std::endl;
        // std::cout << "seekByteOffset:" << seekByteOffset << std::endl;
    }
}
