#include "H264FramedSource.hpp"

namespace LIRS {

    H264FramedSource *H264FramedSource::createNew(UsageEnvironment &env, Transcoder *transcoder) {
        return new H264FramedSource(env, transcoder);
    }

    H264FramedSource::~H264FramedSource() {
        envir().taskScheduler().deleteEventTrigger(eventTriggerId);
        eventTriggerId = 0;
        LOG(DEBUG) << "H264 Camera Framed Source has been destructed";
    }

    H264FramedSource::H264FramedSource(UsageEnvironment &env, Transcoder *transcoder) :
            FramedSource(env), transcoder(transcoder), eventTriggerId(0) {

        eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);

        LOG(DEBUG) << "Event trigger ID for deliverFrame0: " << eventTriggerId;

        transcoder->setOnFrameCallback(std::bind(&H264FramedSource::onFrame, this));

        std::thread([transcoder](){
            transcoder->playVideo();
        }).detach();

        std::thread([transcoder](){
            transcoder->fetchFrames();
        }).detach();
    }

    void H264FramedSource::doGetNextFrame() {
        deliverFrame();
    }

    void H264FramedSource::deliverFrame() {

        if (!isCurrentlyAwaitingData()) {
            return;
        }

        std::vector<uint8_t> frame;
        auto status = transcoder->retrieveEncodedData(frame);

        if (status) {

            if (frame.size() > fMaxSize) {
                fFrameSize = fMaxSize;
                fNumTruncatedBytes = static_cast<unsigned int>(frame.size() - fMaxSize);
            } else {
                fFrameSize = static_cast<unsigned int>(frame.size());
            }

            gettimeofday(&fPresentationTime, nullptr);
            memmove(fTo, frame.data(), fFrameSize); // DO NOT CHANGE ADDRESS, ONLY COPY
            FramedSource::afterGetting(this);

        } else {
            fFrameSize = 0;
        }
    }

    void H264FramedSource::onFrame() {
        envir().taskScheduler().triggerEvent(eventTriggerId, this);
    }

    void H264FramedSource::deliverFrame0(void *p) {
        ((H264FramedSource*)p)->deliverFrame();
    }

}



