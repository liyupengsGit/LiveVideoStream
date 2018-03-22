#include "USBCamFramedSource.hpp"

namespace LIRS {

    USBCamFramedSource *USBCamFramedSource::createNew(UsageEnvironment &env, Transcoder *transcoder) {
        return new USBCamFramedSource(env, transcoder);
    }

    USBCamFramedSource::~USBCamFramedSource() {

        // delete resources
        delete transcoder;

        // delete trigger
        envir().taskScheduler().deleteEventTrigger(eventTriggerId);
        eventTriggerId = 0;

        LOG(DEBUG) << "H264 Camera Framed Source has been destructed";
    }

    USBCamFramedSource::USBCamFramedSource(UsageEnvironment &env, Transcoder *transcoder) :
            FramedSource(env), transcoder(transcoder), eventTriggerId(0) {

        // create trigger invoking method which will deliver frame
        eventTriggerId = envir().taskScheduler().createEventTrigger(USBCamFramedSource::deliverFrame0);

        // set transcoder's callback indicating new encoded data availability to the onEncodedData function
        transcoder->setOnEncodedDataCallback(
                std::bind(&USBCamFramedSource::onEncodedData, this, std::placeholders::_1));

        // start video data encoding and decoding

        std::thread([transcoder]() {
            transcoder->run();
        }).detach();
    }

//    void USBCamFramedSource::doGetNextFrame() {}


    void USBCamFramedSource::onEncodedData(std::vector<uint8_t> &&newData) {

        if (!isCurrentlyAwaitingData()) return;

        if (encodedData.size() >= 5) {
            encodedData.clear();
        }

        encodedData.emplace_back(std::move(newData));

        // publish an event to be handled by the event loop
        envir().taskScheduler().triggerEvent(eventTriggerId, this);
    }

    void USBCamFramedSource::deliverFrame0(void *clientData) {
        LOG(WARN) << R"(Call "doGetNextFrame")";
        ((USBCamFramedSource *) clientData)->deliverData();
    }

    void USBCamFramedSource::doStopGettingFrames() {
        FramedSource::doStopGettingFrames();
    }

    void USBCamFramedSource::deliverData() {

        if (!isCurrentlyAwaitingData()) {
            return; // there's no consumers (clients)
        }

        if (encodedData.empty()) {
            return;
        }

        auto frame = encodedData.back();
        encodedData.pop_back();

        if (frame.size() > fMaxSize) { // truncate data
            fFrameSize = fMaxSize;
            fNumTruncatedBytes = static_cast<unsigned int>(frame.size() - fMaxSize);
        } else {
            fFrameSize = static_cast<unsigned int>(frame.size());
        }

        gettimeofday(&fPresentationTime, nullptr); // can be changed to the actual frame's captured time

        memcpy(fTo, frame.data(), fFrameSize); // DO NOT CHANGE ADDRESS, ONLY COPY (see Live555 docs)

        FramedSource::afterGetting(this); // should be invoked after successfully getting data
    }

    void USBCamFramedSource::doGetNextFrame() {
        // nothing
    }
}



