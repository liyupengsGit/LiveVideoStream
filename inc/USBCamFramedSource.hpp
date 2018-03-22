#ifndef LIVE_VIDEO_STREAM_H264_FRAMED_SOURCE_HPP
#define LIVE_VIDEO_STREAM_H264_FRAMED_SOURCE_HPP

#include <FramedSource.hh>
#include <UsageEnvironment.hh>
#include <Transcoder.hpp>
#include <list>

namespace LIRS {

    /**
     * Implementation of the source in Live555 framework.
     */
    class USBCamFramedSource : public FramedSource {
    public:

        static USBCamFramedSource *createNew(UsageEnvironment &env, Transcoder *transcoder);

    protected:

        /**
         * Constructs new instance of framed source using video device as a source.
         *
         * @param env - environment (see Live555 docs).
         * @param transcoder - pointer to the transcoder providing encoded data.
         */
        USBCamFramedSource(UsageEnvironment &env, Transcoder *transcoder);

        ~USBCamFramedSource() override;

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

        std::vector<std::vector<uint8_t>> encodedData;

        /**
         * Function to be called when the video source has a new available encoded data.
         */
        void onEncodedData(std::vector<uint8_t>&& data);

        void deliverData();

        static void deliverFrame0(void *);
    };

}

#endif //LIVE_VIDEO_STREAM_H264_FRAMED_SOURCE_HPP
