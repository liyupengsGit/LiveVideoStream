#ifndef LIVE_USB_CAM_FRAMED_SOURCE_HPP
#define LIVE_USB_CAM_FRAMED_SOURCE_HPP

#include <FramedSource.hh>
#include <UsageEnvironment.hh>
#include <Transcoder.hpp>
#include <list>

namespace LIRS {

    /**
     * Implementation of the framed source for live usb camera.
     */
    class LiveUSBCamFramedSource : public FramedSource {
    public:

        static LiveUSBCamFramedSource *createNew(UsageEnvironment &env, Transcoder *transcoder);

    protected:

        /**
         * Constructs new instance of framed source using video device as a source.
         *
         * @param env - environment (see Live555 docs).
         * @param transcoder - providing with encoded data.
         */
        LiveUSBCamFramedSource(UsageEnvironment &env, Transcoder *transcoder);

        ~LiveUSBCamFramedSource() override;

        void doGetNextFrame() override;

        void doStopGettingFrames() override;

    private:

        /**
         * Provides encoded data from the video device it represents.
         */
        Transcoder *transcoder;

        /*
         * Indicating an event invoking deliver frame method.
         */
        EventTriggerId eventTriggerId;

        std::mutex encodedDataMutex;

        /**
         * Encoded data buffer.
         * This data will be sent by the server.
         */
        std::vector<std::vector<uint8_t>> encodedData;

        /**
         * Function to be called when the video source has a new available encoded data.
         */
        void onEncodedData(std::vector<uint8_t> &&data);

        /**
         * Delivers encoded data.
         */
        void deliverData();

        /**
         * Trigger binded function.
         * It will be called by the trigger.
         */
        static void deliverFrame0(void *);
    };

}

#endif //LIVE_USB_CAM_FRAMED_SOURCE_HPP
