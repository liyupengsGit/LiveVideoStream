#include "H264FramedSource.hpp"

namespace LIRS {

    H264FramedSource *H264FramedSource::createNew(UsageEnvironment &env, Transcoder *transcoder) {
        return new H264FramedSource(env, transcoder);
    }

    H264FramedSource::~H264FramedSource() {

        // delete trigger
        envir().taskScheduler().deleteEventTrigger(eventTriggerId);
        eventTriggerId = 0;

        LOG(DEBUG) << "H264 Camera Framed Source has been destructed";
    }

    H264FramedSource::H264FramedSource(UsageEnvironment &env, Transcoder *transcoder) :
            FramedSource(env), transcoder(transcoder), eventTriggerId(0) {

        // create trigger invoking method which will deliver frame
        eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);

        LOG(DEBUG) << "Event trigger ID for deliverFrame0: " << eventTriggerId;

        // set transcoder's callback indicating new encoded data availability to the onEncodedData function
        transcoder->setOnEncodedDataCallback(std::bind(&H264FramedSource::onEnCodedData, this));

        // start video data encoding and decoding threads

        std::thread([transcoder]() {
            transcoder->runDecoder();
        }).detach();

        std::thread([transcoder]() {
            transcoder->runEncoder();
        }).detach();
    }

    void H264FramedSource::doGetNextFrame() {
        deliverFrame();
    }

    void H264FramedSource::deliverFrame() {

        if (!isCurrentlyAwaitingData()) {
            return; // don't ready to process new data
        }

        // get encoded data
        auto status = transcoder->retrieveEncodedData(frame);

        if (status) {

            if (frame.size() > fMaxSize) { // truncate data
                fFrameSize = fMaxSize;
                fNumTruncatedBytes = static_cast<unsigned int>(frame.size() - fMaxSize);
            } else {
                fFrameSize = static_cast<unsigned int>(frame.size());
            }

            gettimeofday(&fPresentationTime, nullptr); // can be changed to the actual frame's captured time

            memmove(fTo, frame.data(), fFrameSize); // DO NOT CHANGE ADDRESS, ONLY COPY (see Live555 docs)

            FramedSource::afterGetting(this); // should be invoked after successfully getting data

        } else {
            fFrameSize = 0; // nothing to deliver
        }

        frame.clear();
    }

    void H264FramedSource::onEnCodedData() {
        // publish an event to be handled by the event loop
        envir().taskScheduler().triggerEvent(eventTriggerId, this);
    }

    void H264FramedSource::deliverFrame0(void *p) {
        ((H264FramedSource *) p)->deliverFrame();
    }

}



