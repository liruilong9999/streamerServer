#ifndef SEEKRTSPSERVER_H
#define SEEKRTSPSERVER_H

#include "httpfileserver.h"
#include "videocommon_global.h"

class TaskScheduler;
class UsageEnvironment;
class RTSPServer;
class UserAuthenticationDatabase;
class VIDEOCOMMON_EXPORT RtspServiceManager
{
public:
    RtspServiceManager();

    ~RtspServiceManager();

    void startRtspServer();

    void stopRtspServer();

private:
    TaskScheduler *              m_scheduler{nullptr};
    UsageEnvironment *           m_env{nullptr};
    RTSPServer *                 m_rtspServer{nullptr};
    UserAuthenticationDatabase * m_authDB{nullptr};
    std::atomic<bool>            m_running{false}; // 标志位，用于控制事件循环
};
#endif
