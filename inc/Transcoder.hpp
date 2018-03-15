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
     * Transcoder enables to decode raw data from video device,
     * encode it using H.264 (libx264) codec, and pass the encoded data to the consumer.
     */
    class Transcoder {

    public:

        /**
         * Creates new instance of this class.
         *
         * @param sourceUrl - video source path/url, e.g. /dev/video0, /dev/video1.
         * @param shortDevName - alias name for the device (optional).
         * @param frameWidth - width of the frame used for decoding and encoding process (could be changed if not supported).
         * @param frameHeight - height of the frame used for decoding and encoding process (could be changed if not supported).
         * @param rawPixelFormatStr - pixel format of the raw video data from the camera, e.g. 'yuyv422', 'bayer_grbg8' (could be changed if not supported).
         * @param encoderPixelFormatStr - pixel format of the encoded data (see supported pixel formats for libx264).
         * @param frameRate - hardware's framerate (could be changed if not supported by the device).
         * @param outputFrameRate - desired framerate (used to decrease device's framerate).
         * @param outputBitRate - desired bitrate for the encoder (libx264, -b flag).
         * @return pointer to the created instance of the transcoder class.
         */
        static Transcoder *
        newInstance(std::string sourceUrl, std::string shortDevName, size_t frameWidth, size_t frameHeight,
                    std::string rawPixelFormatStr, std::string encoderPixelFormatStr,
                    size_t frameRate, size_t outputFrameRate, size_t outputBitRate);

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
         * Decodes video data from video device.
         * The device's supported framerate is used.
         */
        void playVideo();

        /**
         * Encodes captured video data.
         * The source's framerate is decreased to the specified value.
         * This function invokes callback which is used in order send signal to the consumer
         * that new encoded video data is arrived, e.g. for server.
         * The data is stored in the internal buffer.
         */
        void fetchFrames();

        /**
         * Retrieves frame from the internal buffer.
         *
         * @param frame - reference to video data to be filled.
         * @return true - if there is some data in the buffer, otherwise - false.
         */
        bool retrieveFrame(std::vector<uint8_t> &frame);

        /**
         * Sets callback function which indicates that a new encoded video frame is available.
         *
         * @param callback - callback function to be set to.
         */
        void setOnFrameCallback(std::function<void()> callback);

        /**
         * Returns device url/path, e.g. /dev/video0.
         *
         * @return device url/path.
         */
        std::string getDeviceName() const;

    private:

        /**
         * Constructs new instance of the transcoder class.
         * Used internally (private).
         * In order to create an instance use newInstance function instead.
         *
         * @param url - video source url/path, e.g. /dev/video0
         * @param shortDevName - device alias name (optional).
         * @param w - frame width (could be changed if not supported).
         * @param h - frame height (could be changed if not supported).
         * @param rawPixFmtStr - raw video data pixel format, e.g. 'yuyv422', 'bayer_grbg8' (could be changed if not supported).
         * @param encPixFmtStr - encoded video data pixel format (see libx264 supported pixel formats).
         * @param frameRate - device's supported framerate (could be changed by device if not supported).
         * @param outFrameRate - the desired framerate (used to decrease supported framerate).
         * @param outBitRate - encoder's bitrate (see libx264, -b flag).
         */
        Transcoder(std::string url, std::string shortDevName, size_t w, size_t h, std::string rawPixFmtStr,
                   std::string encPixFmtStr, size_t frameRate, size_t outFrameRate, size_t outBitRate);

        /* parameters */

        /**
         * Video source url/path, e.g. /dev/video0.
         */
        std::string videoSourceUrl;

        /**
         * Device name alias.
         */
        std::string shortDeviceName;

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
         * Encoded video data pixel format (libx264).
         */
        AVPixelFormat encoderPixFormat;

        /**
         * Device's supported framerate.
         */
        size_t frameRate;

        /**
         * Desired framerate.
         * Supported framerate is decreased to this value.
         */
        size_t outputFrameRate;

        /**
         * Video source's bitrate (raw video data).
         */
        size_t sourceBitRate;

        /**
         * Encoded video data bitrate (encoder).
         */
        size_t outputBitRate;

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
         * Packet sent to the decoder in order to receive raw frame.
         */
        AVPacket *decodingPacket;

        /**
         * Packet holding the encoded data (H.264).
         */
        AVPacket *encodingPacket;

        /**
         * Context holding information on conversion from one pixel format to another one.
         * For example, 'yuyv422' -> 'yuv422p'.
         */
        SwsContext *converterContext;

        /**
         * Mutex used to give concurrent access to the queue holding arriving encoded frames from the encoder.
         */
        std::mutex outQueueMutex;

        /**
         * Queue used to hold encoded data from the encoder.
         * Encoded frames are retrieved from this queue by the consumer.s
         */
        std::queue<std::vector<uint8_t>> outQueue;

        /**
         * Mutex used to control the framerate.
         * It locks/unlocks access to the raw frame data (see rawFrame).
         */
        std::mutex fetchLastFrameMutex;

        /**
         * Flag indicating whether the device is accessible or not.
         */
        std::atomic_bool isPlayingFlag;

        /** constants **/

        /**
         * H.264 start code bytes number (first 4 bytes).
         * Used to truncate start codes from the encoded data.
         */
        const static unsigned int H264_START_CODE_BYTES_NUMBER = 4;

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
         * Initializes encoder in order to encode raw frames using H.264 codec.
         * Tune encoder here using different profiles, tune options, crf values etc.
         * See more on options in H.264 docs.
         */
        void initializeEncoder();

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
         * Initializes converter from raw pixel format to the encoder supported pixel format.
         */
        void initializeConverter();

        /* Parameters */

        /**
         * Callback function called when new encoded video data is available.
         */
        std::function<void()> onFrameCallback;
    };
}

#endif //LIVE_VIDEO_STREAM_TRANSCODER_HPP
