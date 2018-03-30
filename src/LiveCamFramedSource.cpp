#include "LiveCamFramedSource.hpp"

namespace LIRS {

    LiveCamFramedSource *LiveCamFramedSource::createNew(UsageEnvironment &env, Transcoder *transcoder) {
        return new LiveCamFramedSource(env, transcoder);
    }

    LiveCamFramedSource::~LiveCamFramedSource() {

        // cleanup transcoder
        delete transcoder;

        // delete trigger
        envir().taskScheduler().deleteEventTrigger(eventTriggerId);
        eventTriggerId = 0;

        // cleanup encoded data buffer
        encodedDataBuffer.clear();
        encodedDataBuffer.shrink_to_fit();

        LOG(DEBUG) << "USB camera framed source " << transcoder->getDeviceName() << " has been destructed";
    }

    LiveCamFramedSource::LiveCamFramedSource(UsageEnvironment &env, Transcoder *transcoder) :
            FramedSource(env), transcoder(transcoder), eventTriggerId(0) {

        // create trigger invoking method which will deliver frame
        eventTriggerId = envir().taskScheduler().createEventTrigger(LiveCamFramedSource::deliverFrame0);

        encodedDataBuffer.reserve(5); // reserve enough space for handling incoming encoded data

        // set transcoder's callback indicating new encoded data availability
        transcoder->setOnEncodedDataCallback(std::bind(&LiveCamFramedSource::onEncodedData, this,
                                                       std::placeholders::_1));

        // start video data encoding/decoding in a new thread

        std::thread([transcoder]() {
            transcoder->run();
        }).detach();
    }

    void LiveCamFramedSource::onEncodedData(std::vector<uint8_t> &&newData) {

        if (!isCurrentlyAwaitingData())
           return;

        { // restrict the scope of the lock

            std::lock_guard<std::mutex> lock_guard(encodedDataMutex);

            encodedDataBuffer.emplace_back(std::move(newData)); // add encoded data to be processed later
        }

        // publish an event to be handled by the event loop
        envir().taskScheduler().triggerEvent(eventTriggerId, this);
    }

    void LiveCamFramedSource::deliverFrame0(void *clientData) {
        ((LiveCamFramedSource *) clientData)->deliverData();
    }

    void LiveCamFramedSource::doStopGettingFrames() {
        FramedSource::doStopGettingFrames();
    }

    void LiveCamFramedSource::deliverData() {

        if (!isCurrentlyAwaitingData()) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock_guard(encodedDataMutex);

            encodedData = std::move(encodedDataBuffer.back());

            encodedDataBuffer.pop_back();
        }

        if (encodedData.size() > fMaxSize) { // truncate data
            fFrameSize = fMaxSize;
            fNumTruncatedBytes = static_cast<unsigned int>(encodedData.size() - fMaxSize);
            LOG(WARN) << "Truncated: " << fNumTruncatedBytes << ", size: " << encodedData.size();
        } else {
            fFrameSize = static_cast<unsigned int>(encodedData.size());
        }

        gettimeofday(&fPresentationTime, nullptr); // can be changed to the actual frame's captured time

        memcpy(fTo, encodedData.data(), fFrameSize); // DO NOT CHANGE ADDRESS, ONLY COPY (see Live555 docs)

        FramedSource::afterGetting(this); // should be invoked after successfully getting data
    }

    void LiveCamFramedSource::doGetNextFrame() {

        if (!encodedDataBuffer.empty()) {
            deliverData();
        } else {
            fFrameSize = 0;
            return;
        }
    }
}



