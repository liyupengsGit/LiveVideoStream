#ifndef LIVE_VIDEO_STREAM_H264_FRAMED_SOURCE_HPP
#define LIVE_VIDEO_STREAM_H264_FRAMED_SOURCE_HPP

#include <FramedSource.hh>
#include <UsageEnvironment.hh>
#include <Transcoder.hpp>

namespace LIRS {

    class H264FramedSource : public FramedSource {
    public:

        static H264FramedSource* createNew(UsageEnvironment& env, Transcoder *transcoder);

    protected:

        H264FramedSource(UsageEnvironment &env, Transcoder *transcoder);

        ~H264FramedSource() override;

        void doGetNextFrame() override;

    private:

        Transcoder *transcoder;
        EventTriggerId eventTriggerId;

        void deliverFrame();

        void onFrame();

        static void deliverFrame0(void* p);
    };

}

#endif //LIVE_VIDEO_STREAM_H264_FRAMED_SOURCE_HPP
