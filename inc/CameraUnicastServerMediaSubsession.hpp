#ifndef LIVE_VIDEO_STREAM_CUSTOM_SERVER_MEDIA_SUBSESSION_HPP
#define LIVE_VIDEO_STREAM_CUSTOM_SERVER_MEDIA_SUBSESSION_HPP

#include <OnDemandServerMediaSubsession.hh>
#include <StreamReplicator.hh>
#include <H264VideoRTPSink.hh>
#include <H264VideoStreamDiscreteFramer.hh>

namespace LIRS {

    /*
     * Live555 unicast server media subsession class implementation.
     */
    class CameraUnicastServerMediaSubsession : public OnDemandServerMediaSubsession {

    public:
        static CameraUnicastServerMediaSubsession *createNew(UsageEnvironment &env, StreamReplicator *replicator);

    protected:

        StreamReplicator *replicator;

        CameraUnicastServerMediaSubsession(UsageEnvironment &env, StreamReplicator *replicator);

        FramedSource *createNewStreamSource(unsigned clientSessionId, unsigned &estBitrate) override;

        RTPSink *createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
                                  FramedSource *inputSource) override;
    };
}

#endif //LIVE_VIDEO_STREAM_CUSTOM_SERVER_MEDIA_SUBSESSION_HPP
