#ifndef SEEKRTSPSERVER_H
#define SEEKRTSPSERVER_H

#ifndef _RTSP_SERVER_SUPPORTING_HTTP_STREAMING_HH
#include "RTSPServerSupportingHTTPStreaming.hh"
#endif

class SeekRTSPServer : public RTSPServerSupportingHTTPStreaming
{
public:
    static SeekRTSPServer * createNew(UsageEnvironment & env, Port ourPort, UserAuthenticationDatabase * authDatabase, unsigned reclamationTestSeconds = 65);

protected:
    SeekRTSPServer(UsageEnvironment & env, int ourSocket, Port ourPort, UserAuthenticationDatabase * authDatabase, unsigned reclamationTestSeconds);
    // called only by createNew();
    virtual ~SeekRTSPServer();

protected: // redefined virtual functions
    virtual ServerMediaSession *
    lookupServerMediaSession(char const * streamName, Boolean isFirstLookupInSession);
};

#endif
