#include "LiveUSBCamFramedSource.hpp"

namespace LIRS {

    LiveUSBCamFramedSource *LiveUSBCamFramedSource::createNew(UsageEnvironment &env, Transcoder *transcoder) {
        return new LiveUSBCamFramedSource(env, transcoder);
    }

    LiveUSBCamFramedSource::~LiveUSBCamFramedSource() {

        // cleanup transcoder
        delete transcoder;

        // delete trigger
        envir().taskScheduler().deleteEventTrigger(eventTriggerId);
        eventTriggerId = 0;

        LOG(DEBUG) << "USB camera framed source " << transcoder->getDeviceName() <<  " has been destructed";
    }

    LiveUSBCamFramedSource::LiveUSBCamFramedSource(UsageEnvironment &env, Transcoder *transcoder) :
            FramedSource(env), transcoder(transcoder), eventTriggerId(0) {

        // create trigger invoking method which will deliver frame
        eventTriggerId = envir().taskScheduler().createEventTrigger(LiveUSBCamFramedSource::deliverFrame0);

        // set transcoder's callback indicating new encoded data availability
        transcoder->setOnEncodedDataCallback(std::bind(&LiveUSBCamFramedSource::onEncodedData,
                                                       this, std::placeholders::_1));

        // start video data encoding and decoding

        std::thread([transcoder]() {
            transcoder->run();
        }).detach();
    }

    void LiveUSBCamFramedSource::onEncodedData(std::vector<uint8_t> &&newData) {

        if (!isCurrentlyAwaitingData()) return;

        encodedDataMutex.lock();

        encodedData.emplace_back(std::move(newData));

        encodedDataMutex.unlock();

        // publish an event to be handled by the event loop
        envir().taskScheduler().triggerEvent(eventTriggerId, this);
    }

    void LiveUSBCamFramedSource::deliverFrame0(void *clientData) {
        ((LiveUSBCamFramedSource *) clientData)->deliverData();
    }

    void LiveUSBCamFramedSource::doStopGettingFrames() {
        FramedSource::doStopGettingFrames();
    }

    void LiveUSBCamFramedSource::deliverData() {

        if (!isCurrentlyAwaitingData()) {
            return; // there's no consumers (clients)
        }

        if (encodedData.empty()) {
            LOG(WARN) << "---";
            return;
        }

        encodedDataMutex.lock();

        auto frame = encodedData.back();

        encodedData.pop_back();

        encodedDataMutex.unlock();

        if (frame.size() > fMaxSize) { // truncate data
            fFrameSize = fMaxSize;
            fNumTruncatedBytes = static_cast<unsigned int>(frame.size() - fMaxSize);
            LOG(WARN) << "Encoded data size is insufficient: " << fMaxSize << ", truncated: " << fNumTruncatedBytes;
        } else {
            fFrameSize = static_cast<unsigned int>(frame.size());
        }

        gettimeofday(&fPresentationTime, nullptr); // can be changed to the actual frame's captured time

        memcpy(fTo, frame.data(), fFrameSize); // DO NOT CHANGE ADDRESS, ONLY COPY (see Live555 docs)

        FramedSource::afterGetting(this); // should be invoked after successfully getting data
    }

    void LiveUSBCamFramedSource::doGetNextFrame() {}
}



