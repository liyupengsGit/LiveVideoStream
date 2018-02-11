#ifndef LIVE_VIDEO_STREAM_FFMPEG_DECODER_H
#define LIVE_VIDEO_STREAM_FFMPEG_DECODER_H

#include "Decoder.h" // abstract base class for decoders
#include <vector>

#ifdef __cplusplus
extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

namespace LIRS {

    /**
     * FFmpeg based decoder.
     */
    class FFmpegDecoder : public Decoder {
    public:

        explicit FFmpegDecoder(std::string videoSourcePath, DecoderParams params = {});

        void decodeLoop() override;

        ~FFmpegDecoder() override;

    private:

        AVCodecContext *pCodecCtx;
        AVFormatContext *pFormatCtx;
        SwsContext *pImageConvertCtx;

        /**
         * Structure that describes decoded (raw) video data.
         * Holds pointers to the decoded frames.
         */
        AVFrame *pFrame;

        /**
         * Video stream index.
         */
        int videoStreamIdx;

        /**
         * Buffer for holding video frame's data.
         */
        std::vector<uint8_t> buffer;

        /**
         * Initializes ffmpeg environment registering all available codecs,
         * formats, devices, etc.
         */
        void registerAll();

        /**
         * Sends packet and retrieves frame with raw image data.
         *
         * @param codecContext codec context.
         * @param frame raw image frame which is to be populated with data.
         * @param gotFrame flag is set to true when a frame was successively captured otherwise it is set to false.
         * @param packet packet to be sent to the decoder.
         * @return 0 - if success, otherwise ffmpeg error code.
         */
        int decodeVideo(AVCodecContext *codecContext, AVFrame *frame, bool &gotFrame, AVPacket *packet);

        /*
         * Opens an input stream using video4linux.
         * Frame rate, width and height are used in order to
         * set stream parameters.
         *
         * @return 0 - on success, otherwise ffmpeg error.
         */
        int openInputStream();

        /**
         * Finds and sets the video stream index parameter.
         * Sets video stream index to -1 in case of when no video stream is found.
         */
        void findVideoInputStream();

        /**
         * Initializes codec context with codec.
         * Allocates codec context, initializes it to use the given codec.
         *
         * @param codecParameters codec parameters.
         * @return 0 - on success, otherwise ffmpeg error.
         */
        int initializeCodecContext(const AVCodecParameters *codecParameters);

        /**
         * Initializes frame.
         * Allocates buffer which will contain image data.
         * Sets up frame's pointer to the buffer.
         *
         * @param pixelFormat image data pixel format, i.e. "yuyv422", "bgr24".
         * @param pFrame frame to be initialized.
         * @return the size in bytes needed for the image, negative ffmpeg error code.
         */
        int initializeFrame(AVPixelFormat pixelFormat, AVFrame *pFrame);

    };
}

#endif //LIVE_VIDEO_STREAM_FFMPEG_DECODER_H
