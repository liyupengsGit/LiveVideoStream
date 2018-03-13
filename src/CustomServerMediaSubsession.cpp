#include "CustomServerMediaSubsession.hpp"

namespace LIRS {

    CustomServerMediaSubsession *CustomServerMediaSubsession::createNew(UsageEnvironment &env,
                                                                        StreamReplicator *replicator) {
        return new CustomServerMediaSubsession(env, replicator);
    }

    CustomServerMediaSubsession::CustomServerMediaSubsession(UsageEnvironment &env, StreamReplicator *replicator)
            : OnDemandServerMediaSubsession(env, False), replicator(replicator) {
    }

    FramedSource *CustomServerMediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned &estBitrate) {
        auto source = replicator->createStreamReplica();
        return H264VideoStreamDiscreteFramer::createNew(envir(), source);
    }

    RTPSink *CustomServerMediaSubsession::createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
                                                  FramedSource *inputSource) {
        return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    }

}


