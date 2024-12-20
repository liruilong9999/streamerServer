#ifndef SEEK_H264_VEDIO_FILE_SERVER_MEDIA_SUBSESSION_H
#define SEEK_H264_VEDIO_FILE_SERVER_MEDIA_SUBSESSION_H

////////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>	A seek h 264 video file server media subsession. </summary>
///
///  重写创建264视频文件的子会话类，相较于 H264VideoFileServerMediaSubsession， 增加了视频进度控制（seek）功能
///
/// <remarks>	Liruilong, 2024/12/20. </remarks>
////////////////////////////////////////////////////////////////////////////////////////////////////

class H264VideoFileServerMediaSubsession;

class SeekH264VideoFileServerMediaSubsession : public H264VideoFileServerMediaSubsession
{
public:
    static SeekH264VideoFileServerMediaSubsession * createNew(UsageEnvironment & env, char const * outFileName, char const * inFileName, Boolean reuseFirstSource);

protected:
    SeekH264VideoFileServerMediaSubsession(UsageEnvironment & env, char const * outFileName, char const * inFileName, Boolean reuseFirstSource);
    virtual ~SeekH264VideoFileServerMediaSubsession();

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>	Gets the duration. </summary>
    ///
    ///		返回视频时长（单位秒）
    ///
    /// <remarks>	Liruilong, 2024/12/20. </remarks>
    ///
    /// <returns>	A float. </returns>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    virtual float duration() const override;

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// <summary>	Seek stream source. </summary>
    ///
    ///		定位到所在时长的文件位置，根据比特率计算
    ///
    /// <remarks>	Liruilong, 2024/12/20. </remarks>
    ///
    /// <param name="inputSource">   	[in,out] If non-null, the input source. </param>
    /// <param name="seekNPT">		 	[in,out] The seek npt. </param>
    /// <param name="streamDuration">	Duration of the stream. </param>
    /// <param name="numBytes">		 	[in,out] Number of bytes. </param>
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    virtual void seekStreamSource(FramedSource * inputSource, double & seekNPT, double streamDuration, u_int64_t & numBytes);

private:
    float    m_fDurationSeconds; // 视频时长（以秒为单位）
    unsigned m_getBitrate;       // 比特率 (单位b/s)
};

#endif //
