#ifndef LIVE_VIDEO_STREAM_FFMPEG_DECODER_H
#define LIVE_VIDEO_STREAM_FFMPEG_DECODER_H

#include "Decoder.h" // abstract base class for decoders
#include <sstream>

#ifdef __cplusplus
extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

/**
 * @todo consider sending flush packets (with NULL data and size), see avcodec_send_packet(..)
 * @todo remove asserts, don't use hardcoded opts, throw exceptions on errors
 *
 */
namespace LIRS {

    /**
     * Decoder class using ffmpeg.
     */
    class FFmpegDecoder : public Decoder {
    public:

        explicit FFmpegDecoder(std::string videoSourcePath, DecoderParams params = {});

        void decodeLoop() override;

        ~FFmpegDecoder() override;

    private:

        // contexts
        AVCodecContext *pCodecCtx;
        AVFormatContext *pFormatCtx;
        SwsContext *pImageConvertCtx;

        // converted frame
        AVFrame *pFrame;

        // index of the video stream
        int videoStreamIdx;

        // frame's buffer
        std::vector<uint8_t> buffer;


        /**
         * Initializes FFmpeg environment registering all available codecs,
         * formats, devices, etc.
         */
        void registerAll();

        /**
         * Sends packet and retrieves frame with raw image data.
         *
         * @param codecContext codec context.
         * @param frame raw image frame which is to be populated with data.
         * @param gotFrame flag is set to true when frame was captured otherwise it is set to false.
         * @param packet packet to be sent to the decoder.
         * @return 0 - if success, otherwise error code.
         */
        int decodeVideo(AVCodecContext *codecContext, AVFrame *frame, bool &gotFrame, AVPacket *packet);

    };
}

#endif //LIVE_VIDEO_STREAM_FFMPEG_DECODER_H
