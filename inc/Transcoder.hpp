#ifndef LIVE_VIDEO_STREAM_TRANSCODER_HPP
#define LIVE_VIDEO_STREAM_TRANSCODER_HPP

#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <chrono>

#include "Utils.hpp"
#include "Logger.hpp"

#ifdef __cplusplus
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}
#endif

namespace LIRS {

    /**
     * Structure representing context for decoding or encoding process.
     */
    typedef struct TranscoderContext {

        /**
         * Input or output context format.
         */
        AVFormatContext *formatContext;

        /**
         * Decoding/encoding context for the decoder or encoder.
         */
        AVCodecContext *codecContext;

        /**
         * Encoder or decoder.
         */
        AVCodec *codec;

        /**
         * Video data stream.
         */
        AVStream *videoStream;

        /**
         * Default constructor.
         * Used in order to set all values to nullptr.
         */
        TranscoderContext() : formatContext(nullptr), codecContext(nullptr), codec(nullptr), videoStream(nullptr) {}

    } TranscoderContext;

    /**
     * Transcoder decodes some video resource and encodes it.
     * The encoded data can be passed to the consumer.
     */
    class Transcoder {

    public:

        /**
         * Creates new instance of this class.
         *
         * @param sourceUrl - video source path/url, e.g. /dev/video0, /dev/video1.
         * @param devAlias - alias name for the device (optional).
         * @param frameWidth - width of the frame used for decoding and encoding process (could be changed if not supported).
         * @param frameHeight - height of the frame used for decoding and encoding process (could be changed if not supported).
         * @param rawPixelFormatStr - pixel format of the raw video data, e.g. 'yuyv422' (could be changed if not supported).
         * @param encoderPixelFormatStr - pixel format of the encoded data (see supported formats).
         * @param frameRate - hardware's framerate (could be changed if not supported by the device).
         * @param frameStep - how many frames to skip to decrease output framerate.
         * @param outputFrameRate - desired framerate (used to decrease device's framerate).
         * @return pointer to the created instance of the transcoder class.
         */
        static Transcoder *
        newInstance(const std::string &sourceUrl, const std::string &devAlias, size_t frameWidth, size_t frameHeight,
                    const std::string &rawPixelFormatStr, const std::string &encoderPixelFormatStr,
                    size_t frameRate, size_t frameStep, size_t outputFrameRate);

        /**
         * Prohibit copy constructor.
         * Video device couldn't be accessed by multiple consumers.
         */
        Transcoder(const Transcoder &) = delete;

        /**
         * Prohibit copy assignment operator.
         * See copy constructor.
         */
        Transcoder &operator=(const Transcoder &) = delete;

        /**
         * Destructor of the transcoder.
         */
        ~Transcoder();

        /**
         * Starts the process of decoding / encoding data from the video source.
         */
        void run();

        /**
         * Sets callback function which indicates that a new encoded video data is available.
         *
         * @param callback - callback function to be set to.
         */
        void setOnEncodedDataCallback(std::function<void(std::vector<uint8_t> &&)> callback);

        /**
         * Returns device url/path, e.g. /dev/video0.
         *
         * @return device url/path.
         */
        std::string getDeviceName() const;

        /**
         * Returns the device alias, e.g. 'frontCamera'.
         *
         * @return device alias.
         */
        std::string getAlias() const;

    private:

        /**
         * Constructs new instance of the transcoder class.
         * Used internally (private).
         * In order to create an instance use 'newInstance' function instead.
         *
         * @param url - video source url/path, e.g. /dev/video0
         * @param alias - device alias name (optional).
         * @param w - frame width (could be changed if not supported).
         * @param h - frame height (could be changed if not supported).
         * @param rawPixFmtStr - raw video data pixel format, e.g. 'yuyv422', 'bayer_grbg8' (could be changed if not supported).
         * @param encPixFmtStr - encoded video data pixel format.
         * @param frameRate - device's supported framerate (could be changed by device if not supported).
         * @param frameStep - how many frames to skip (used for decreasing framerate).
         * @param outFrameRate - the desired framerate (used to decrease supported framerate).
         */
        Transcoder(const std::string &url, const std::string &alias, size_t w, size_t h,
                   const std::string &rawPixFmtStr, const std::string &encPixFmtStr, size_t frameRate,
                   size_t frameStep, size_t outFrameRate);

        /* parameters */

        /**
         * Video source url/path, e.g. /dev/video0.
         */
        const std::string videoSourceUrl;

        /**
         * Device name alias.
         */
        std::string deviceAlias;

        /**
         * Frame width.
         */
        size_t frameWidth;

        /**
         * Frame height.
         */
        size_t frameHeight;

        /**
         * Raw video data's pixel format.
         */
        AVPixelFormat rawPixFormat;

        /**
         * Encoded video data pixel format.
         */
        AVPixelFormat encoderPixFormat;

        /**
         * Device's supported framerate.
         */
        AVRational frameRate;

        /**
         * Number of frames to skip when encoding.
         */
        size_t frameStep;

        /**
         * Desired framerate.
         * Supported framerate is decreased to this value.
         */
        AVRational outputFrameRate;

        /**
         * Video source's bitrate (raw video data).
         */
        size_t sourceBitRate;

        /**
         * Decoder video context.
         * Used for decoding.
         */
        TranscoderContext decoderContext;

        /**
         * Encoder video context.
         * Used for encoding.
         */
        TranscoderContext encoderContext;

        /**
         * Holds raw frame data and additional information (resolution, etc.)
         */
        AVFrame *rawFrame;

        /**
         * Converted from raw data pixel format to the supported by the encoder pixel format structure.
         */
        AVFrame *convertedFrame;

        /**
         * Frame retrieved from the filter.
         */
        AVFrame *filterFrame;

        /**
         * Packet sent to the decoder in order to receive raw frame.
         */
        AVPacket *decodingPacket;

        /**
         * Packet holding the encoded data.
         */
        AVPacket *encodingPacket;

        /**
         * Context holding information on conversion from one pixel format to another one.
         * For example, 'yuyv422' -> 'yuv422p'.
         */
        SwsContext *converterContext;

        /**
         * Filter graph used to create complex filter chains.
         */
        AVFilterGraph *filterGraph;

        /**
         * Filter context for buffer source.
         */
        AVFilterContext *bufferSrcCtx;

        /**
         * Filter context for buffer sink.
         */
        AVFilterContext *bufferSinkCtx;

        /**
         * Flag indicating whether the device is accessible or not.
         */
        std::atomic_bool isPlayingFlag;

        /**
         * Callback function called when new encoded video data is available.
         */
        std::function<void(std::vector<uint8_t> &&)> onEncodedDataCallback;

        /** constants **/

        /**
         * NALU start code bytes number (first 4 bytes).
         * Used to truncate start codes from the encoded data.
         */
        const static unsigned int NALU_START_CODE_BYTES_NUMBER = 4;

        /* Methods */

        /**
         * Registers ffmpeg codecs, etc.
         */
        void registerAll();

        /**
         * Initializes decoder in order to capture raw frames from the video device.
         */
        void initializeDecoder();

        /**
         * Initializes encoder in order to encode raw frames.
         * Tune encoder here using different profiles, tune options, crf values etc.
         */
        void initializeEncoder();

        /**
         * Initializes converter from raw pixel format to the encoder supported pixel format.
         */
        void initializeConverter();

        /**
         * Initializes filters, e.g. 'framestep=step=3'
         */
        void initFilters();

        /**
         * Captures raw video frame from the device.
         *
         * @param codecContext - codec context (for decoding).
         * @param frame - captured raw video frame.
         * @param packet - packet to be sent to the decoder in order to get frame.
         * @return >= 0 if success, otherwise error occurred.
         */
        int decode(AVCodecContext *codecContext, AVFrame *frame, AVPacket *packet);

        /**
         * Encodes raw frame and stores encode data in packet.
         *
         * @param codecContext - codec context (for encoding).
         * @param frame - raw frame to be sent to the encoder.
         * @param packet - packet with encoded data set by the encoder.
         * @return >= 0 - success, othwerwise error occurred.
         */
        int encode(AVCodecContext *codecContext, AVFrame *frame, AVPacket *packet);

        /**
         * Close all resources, free allocated memory, etc.
         */
        void cleanup();

    };
}

#endif //LIVE_VIDEO_STREAM_TRANSCODER_HPP
