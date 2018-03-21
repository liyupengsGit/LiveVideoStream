#include <Logger.hpp>
#include "CameraUnicastServerMediaSubsession.hpp"

namespace LIRS {

    CameraUnicastServerMediaSubsession *CameraUnicastServerMediaSubsession::createNew(UsageEnvironment &env,
                                                                                      StreamReplicator *replicator) {
        return new CameraUnicastServerMediaSubsession(env, replicator);
    }

    CameraUnicastServerMediaSubsession::CameraUnicastServerMediaSubsession(UsageEnvironment &env,
                                                                           StreamReplicator *replicator)
            : OnDemandServerMediaSubsession(env, False), replicator(replicator) {
    }

    FramedSource *
    CameraUnicastServerMediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned &estBitrate) {

        LOG(INFO) << "Create new stream source for client: " << clientSessionId;

        auto source = replicator->createStreamReplica();

        // using stream discrete framer (because one unit of data is one NALU without start codes)
        return H265VideoStreamDiscreteFramer::createNew(envir(), source);
    }

    RTPSink *
    CameraUnicastServerMediaSubsession::createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
                                                         FramedSource *inputSource) {

        LOG(INFO) << "Create new RTP sink";

        return H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    }

}


