#ifndef LIVE_VIDEO_STREAM_TRANSCODER_HPP
#define LIVE_VIDEO_STREAM_TRANSCODER_HPP

#include <atomic>
#include <functional>
#include <string>

#include "Logger.hpp"
#include "Utils.hpp"
//#include "Config.hpp"

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
     * @note: user must manually destruct this structure.
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
         * Creates a new instance of this class.
         *
         * @param sourceUrl - video source path, e.g. /dev/video0, /dev/video1.
         * @param devAlias - alias name for the device.
         * @param frameWidth - width of the frame used for decoding and encoding process (could be changed if not supported).
         * @param frameHeight - height of the frame used for decoding and encoding process (could be changed if not supported).
         * @param rawPixelFormatStr - pixel format of the raw video data, e.g. 'yuyv422' (could be changed if not supported).
         * @param encoderPixelFormatStr - pixel format of the encoded data (see supported formats).
         * @param frameRate - hardware's framerate (could be changed if not supported by the device).
         * @param outputFrameRate - output framerate of the video stream.
         * @param filterQuery - filter query to create filter graph.
         * @return pointer to the created instance of the transcoder class.
         */
        static Transcoder *
        newInstance(const std::string &sourceUrl, const std::string &devAlias, size_t frameWidth, size_t frameHeight,
                    const std::string &rawPixelFormatStr, const std::string &encoderPixelFormatStr,
                    size_t frameRate, size_t outputFrameRate, const std::string &filterQuery = {});

        /**
         * Prohibit copy constructor.
         * Video device couldn't be accessed by multiple consumers.
         */
        Transcoder(const Transcoder &) = delete;

        /**
         * Prohibit copy assignment operator.
         * @see copy constructor.
         */
        Transcoder &operator=(const Transcoder &) = delete;

        /**
         * Destructor of the transcoder.
         * @note the actual destruction occurs in the cleanup().
         */
        ~Transcoder();

        /**
         * Starts the process of decoding / encoding data from the video source.
         */
        void run();

        /**
         * Sets callback function which indicates that a new encoded video data is available.
         *
         * @param callback - callback function.
         */
        void setOnEncodedDataCallback(std::function<void(std::vector<uint8_t> &&)> callback);

        /**
         * Returns path to the device, e.g. /dev/video0.
         *
         * @return deivce path.
         */
        std::string getDeviceName() const;

        /**
         * Returns the alias of the device, e.g. 'frontCamera'.
         *
         * @return device alias.
         */
        std::string getAlias() const;

        /**
         * Whether the resource is ready for producing encoded data or not.
         *
         * @return true if the resource is ready, otherwise - false.
         */
        const bool isReadable() const;

    private:

        Transcoder(const std::string &url, const std::string &alias, size_t w, size_t h,
                   const std::string &rawPixFmtStr, const std::string &encPixFmtStr, size_t frameRate,
                   size_t outFrameRate, const std::string &filterQuery);

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
         * Output stream's framerate.
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
         * Holds raw video data and additional information (resolution, etc.)
         */
        AVFrame *rawFrame;

        /**
         * Holds converted frame data (from one pixel format to another one).
         */
        AVFrame *convertedFrame;

        /**
         * Frame retrieved from the filter.
         */
        AVFrame *filterFrame;

        /**
         * Decoding packet (is sent to the decoder).
         */
        AVPacket *decodingPacket;

        /**
         * Encoding packet (holds encoded data).
         */
        AVPacket *encodingPacket;

        /**
         * Conversion context from one pixel format to another one.
         */
        SwsContext *converterContext;

        /**
         * Filter query.
         * Filter graph is constructed from it.
         */
        std::string filterQuery;

        /**
         * Filter graph used to create complex filter chains.
         */
        AVFilterGraph *filterGraph;

        /**
         * Filter context for buffer source.
         * Frames to be filtered are passed to the buffer source.
         */
        AVFilterContext *bufferSrcCtx;

        /**
         * Filter context for buffer sink.
         * Frames to be filtered are retrieved from the buffer sink.
         */
        AVFilterContext *bufferSinkCtx;

        /**
         * Flag indicating whether the streaming source is accessible or not.
         * @note used to handle the destruction of the transcoder.
         */
        std::atomic_bool isPlayingFlag;

        /**
         * Callback function called when new encoded video data is available.
         */
        std::function<void(std::vector<uint8_t> &&)> onEncodedDataCallback;

        /**
         * Output frame width after scaling.
         */
        size_t outFrameWidth;

        /**
         * Output frame height after scaling.
         */
        size_t outFrameHeight;

        /** constants **/

        /**
         * NALU start code bytes number (first 4 bytes, {0x0, 0x0, 0x0, 0x1}).
         * Used to truncate start codes from the encoded data.
         */
        constexpr static unsigned int NALU_START_CODE_BYTES_NUMBER = 4U;

        /* Methods */

        /**
         * Registers ffmpeg codecs, etc.
         */
        void registerAll();

        /**
         * Initializes decoder in order to capture raw frames from the video source.
         */
        void initializeDecoder();

        /**
         * Initializes encoder in order to encode raw frames.
         * Tune encoder here using different profiles, tune options.
         */
        void initializeEncoder();

        /**
         * Initializes converter from raw pixel format to the encoder supported pixel format.
         */
        void initializeConverter();

        /**
         * Initializes filters, e.g. 'framestep', 'fps'.
         * See filter docs.
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
