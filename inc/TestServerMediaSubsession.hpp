#ifndef LIVEVIDEOSTREAM_TESTSERVERMEDIASUBSESSION_HPP
#define LIVEVIDEOSTREAM_TESTSERVERMEDIASUBSESSION_HPP

#include <PassiveServerMediaSubsession.hh>
#include <string>
#include <H265VideoStreamDiscreteFramer.hh>
#include <H265VideoRTPSink.hh>
#include <H265VideoStreamFramer.hh>

class MulticastServerMediaSubsession : public PassiveServerMediaSubsession {

public:
    static MulticastServerMediaSubsession* createNew(UsageEnvironment &env, in_addr destAddr, Port rtpPort, Port rtcpPort,
                                                     u_int8_t ttl, u_char rtpPayloadType, FramedSource* source) {

        FramedSource *videoSource = H265VideoStreamDiscreteFramer::createNew(env, source);

        auto rtpGroupsock = new Groupsock(env, destAddr, rtpPort, ttl);
        auto rtcpGroupsock = new Groupsock(env, destAddr, rtcpPort, ttl);

        RTPSink* videoSink = H265VideoRTPSink::createNew(env, rtpGroupsock, rtpPayloadType);

        const unsigned maxCNAMElen = 100;
        u_char CNAME[maxCNAMElen + 1];
        gethostname((char*) CNAME, maxCNAMElen);
        CNAME[maxCNAMElen] = '\0';
        RTCPInstance *rtcpInstance = RTCPInstance::createNew(env, rtcpGroupsock, 500, CNAME, videoSink, nullptr);

        videoSink->startPlaying(*videoSource, nullptr, nullptr);

        return new MulticastServerMediaSubsession(source, videoSink, rtcpInstance);
    }

protected:
    MulticastServerMediaSubsession(FramedSource* source, RTPSink* rtpSink, RTCPInstance* rtcpInstance)
            : PassiveServerMediaSubsession(*rtpSink, rtcpInstance), source(source), rtpSink(rtpSink){}

    FramedSource* source;
    RTPSink *rtpSink;
};

#endif //LIVEVIDEOSTREAM_TESTSERVERMEDIASUBSESSION_HPP
