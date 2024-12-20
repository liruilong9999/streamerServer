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

#include "seekh264videofileservermediasubsession.h"

#ifdef _WIN32
#define popen  _popen
#define pclose _pclose
#endif

// 获取视频时长
float getVideoDuration(const std::string & fileName)
{
    std::string command = "ffprobe -i \"" + fileName + "\" -show_entries format=duration -v quiet -of csv=p=0";
    FILE *      pipe    = popen(command.c_str(), "r");
    if (!pipe)
    {
        std::cerr << "Failed to run ffprobe command" << std::endl;
        return -1.0;
    }

    char        buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        result += buffer;
    }

    pclose(pipe);

    // Trim the result
    result.erase(result.find_last_not_of(" \n\r\t") + 1);

    // Convert result to float
    std::istringstream iss(result);
    float              duration;
    if (!(iss >> duration))
    {
        std::cerr << "Failed to parse duration from ffprobe output: " << result << std::endl;
        return -1.0;
    }

    return duration; // 返回时长（单位：秒）
}

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

// 获取视频比特率
unsigned getBitrate(const std::string & filename)
{
    std::string command = "ffmpeg -i " + filename + " 2>&1";
    FILE *      pipe    = popen(command.c_str(), "r");
    if (!pipe)
    {
        std::cerr << "Failed to run ffmpeg command" << std::endl;
        return 0;
    }

    char        buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        result += buffer;
    }

    pclose(pipe);

    std::regex  bitrate_regex(R"((\d+) kb/s)");
    std::smatch match;
    if (std::regex_search(result, match, bitrate_regex))
    {
        return std::stoi(match[1]);
    }

    std::cerr << "Failed to parse bitrate" << std::endl;
    return 0;
}

SeekH264VideoFileServerMediaSubsession::SeekH264VideoFileServerMediaSubsession(
    UsageEnvironment & env, char const * outFileName, char const * inFileName, Boolean reuseFirstSource)
    : H264VideoFileServerMediaSubsession(env, outFileName, reuseFirstSource)
    , m_fDurationSeconds(0.0)
    , m_getBitrate(0)
{
    // 通过 ffprobe 或其他方法计算视频时长
    m_fDurationSeconds = getVideoDuration(inFileName);
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
        std::cout << "numBytes:" << numBytes << std::endl;
        std::cout << "byteSource->fileSize():" << byteSource->fileSize() << std::endl;
        std::cout << "seekByteOffset:" << seekByteOffset << std::endl;
    }
}
