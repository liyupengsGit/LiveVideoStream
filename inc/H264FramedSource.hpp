#ifndef LIVE_VIDEO_STREAM_H264_FRAMED_SOURCE_HPP
#define LIVE_VIDEO_STREAM_H264_FRAMED_SOURCE_HPP

#include <FramedSource.hh>
#include <UsageEnvironment.hh>
#include <Transcoder.hpp>

namespace LIRS {

    /**
     * Implementation of the source in Live555 framework.
     */
    class H264FramedSource : public FramedSource {
    public:

        static H264FramedSource* createNew(UsageEnvironment& env, Transcoder *transcoder);

    protected:

        /**
         * Constructs new instance of framed source using video device as a source.
         *
         * @param env - environment (see Live555 docs).
         * @param transcoder - pointer to the transcoder providing encoded data.
         */
        H264FramedSource(UsageEnvironment &env, Transcoder *transcoder);

        ~H264FramedSource() override;

        void doGetNextFrame() override;

    private:

        /**
         * Provides H.264 encoded data from the video device it represents.
         */
        Transcoder *transcoder;

        /*
         * Indicating an event invoking deliver frame method.
         */
        EventTriggerId eventTriggerId;

        /**
         * Holds encoded data.
         */
        std::vector<uint8_t> frame;

        /**
         * Fills all necessary fields in order to successfully transmit data over network.
         */
        void deliverFrame();

        /**
         * Function to be called when the video source has a new available encoded data.
         */
        void onEnCodedData();

        static void deliverFrame0(void* p);
    };

}

#endif //LIVE_VIDEO_STREAM_H264_FRAMED_SOURCE_HPP
