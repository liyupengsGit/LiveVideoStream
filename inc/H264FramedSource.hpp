#ifndef LIVE_VIDEO_STREAM_H264_FRAMED_SOURCE_HPP
#define LIVE_VIDEO_STREAM_H264_FRAMED_SOURCE_HPP

#include <functional>
#include <FramedSource.hh>
#include <UsageEnvironment.hh>
#include <Groupsock.hh>
#include <Transcoder.hpp>
#include <utility>
#include "Logger.hpp"

// TODO fix shared_ptr usage
namespace LIRS {

    class H264FramedSource : public FramedSource {
    public:

        static H264FramedSource* createNew(UsageEnvironment& env, Transcoder *transcoder) {
            return new H264FramedSource(env, transcoder);
        }

        ~H264FramedSource() override {
            envir().taskScheduler().deleteEventTrigger(eventTriggerId);
            eventTriggerId = 0;
            LOG(INFO) << "FramedSource destructor";
        }

    protected:

        H264FramedSource(UsageEnvironment &env, Transcoder *transcoder) :
                FramedSource(env), transcoder(transcoder) {

            eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
            transcoder->setOnFrameCallback(std::bind(&H264FramedSource::onFrame, this));
        }

    private:
        Transcoder *transcoder;
        EventTriggerId eventTriggerId;

        void doGetNextFrame() override {
            deliverFrame();
        }

        static void deliverFrame0(void* p) {
            ((H264FramedSource*)p)->doGetNextFrame();
        }

        void deliverFrame() {

            if (!isCurrentlyAwaitingData()) {
//                envir() << "No data...\n";
                return;
            }

            std::vector<uint8_t> frame;
            auto status = transcoder->retrieveFrame(frame);

            if (status) {

                if (frame.size() > fMaxSize) {
                    fFrameSize = fMaxSize;
                    fNumTruncatedBytes = static_cast<unsigned int>(frame.size() - fMaxSize);
                } else {
                    fFrameSize = static_cast<unsigned int>(frame.size());
                }

                gettimeofday(&fPresentationTime, nullptr);
                memcpy(fTo, frame.data(), fFrameSize); // DO NOT CHANGE ADDRESS, ONLY COPY
                FramedSource::afterGetting(this);

            } else {
                fFrameSize = 0;
            }
        }

        void doStopGettingFrames() override {
            FramedSource::doStopGettingFrames();
        }

        void onFrame() {
            envir().taskScheduler().triggerEvent(eventTriggerId, this);
        }
    };


}

#endif //LIVE_VIDEO_STREAM_H264_FRAMED_SOURCE_HPP
