#ifndef SEEK_H264_VEDIO_FILE_SERVER_MEDIA_SUBSESSION_H
#define SEEK_H264_VEDIO_FILE_SERVER_MEDIA_SUBSESSION_H

#include "videocommon_global.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	A seek h 264 video file server media subsession. </summary>
///
///  重写创建264视频文件的子会话类，相较于 H264VideoFileServerMediaSubsession， 增加了视频进度控制（seek）功能
///
/// <remarks>	Liruilong, 2024/12/20. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

class H264VideoFileServerMediaSubsession;

class VIDEOCOMMON_EXPORT SeekH264VideoFileServerMediaSubsession : public H264VideoFileServerMediaSubsession
{
public:
    static SeekH264VideoFileServerMediaSubsession * createNew(UsageEnvironment & env, char const * outFileName, char const * inFileName, Boolean reuseFirstSource);

protected:
    SeekH264VideoFileServerMediaSubsession(UsageEnvironment & env, char const * outFileName, char const * inFileName, Boolean reuseFirstSource);
    virtual ~SeekH264VideoFileServerMediaSubsession();

    /// <summary>
    /// 返回视频时长（单位秒）
    /// </summary>
    /// <returns></returns>
    virtual float duration() const override;

    /// <summary>
    ///  定位到所在时长的文件位置，根据比特率计算
    /// </summary>
    /// <param name="inputSource"></param>
    /// <param name="seekNPT"></param>
    /// <param name="streamDuration"></param>
    /// <param name="numBytes"></param>
    virtual void seekStreamSource(FramedSource * inputSource, double & seekNPT, double streamDuration, u_int64_t & numBytes);

private:
    float    m_fDurationSeconds{0.0}; // 视频时长（以秒为单位）
    unsigned m_getBitrate{1000};       // 比特率 (单位b/s)
};

#endif //
