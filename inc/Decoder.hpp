#ifndef LIVE_VIDEO_STREAM_DECODER_HPP
#define LIVE_VIDEO_STREAM_DECODER_HPP

#include <string>
#include <functional>

namespace LIRS {

    /**
     * Decoder parameters.
     */
    typedef struct DecoderParams {
        size_t frameHeight;
        size_t frameWidth;
        size_t frameRate;
    } DecoderParams;

    /**
     * Abstract class that captures/decodes frames from the specified video source.
     */
    class Decoder {
    public:

        /**
         * Creates decoder with the specified video source and parameters.
         *
         * @param videoSourcePath path to the video source, i.e. "/dev/video0".
         * @param params parameters of the decoder, i.e. frame rate.
         */
        explicit Decoder(std::string videoSourcePath, DecoderParams params = {}) :
                _videoSourcePath(std::move(videoSourcePath)),
                _frameHeight(params.frameHeight), _frameWidth(params.frameWidth),
                _frameRate(params.frameRate), _bitRate(0) {}

        /**
         * Loop capturing video frames and invoking the specified callback.
         */
        virtual void decodeLoop() = 0;

        // getters and setters
        const std::string getVideoSourcePath() const;

        size_t getFrameHeight() const;

        size_t getFrameWidth() const;

        size_t getFrameRate() const;

        size_t getBitRate() const;

        void setOnFrameCallback(const std::function<void(uint8_t *)> &onFrameCallback);
        // end of getters and setters

        /**
         * Nothing to say, it's simply destructor.
         */
        virtual ~Decoder() = default;

        // restrict move & copy constructor and assignment operator
        Decoder(Decoder &&) = delete;
        Decoder(const Decoder &) = delete;
        Decoder &operator=(const Decoder &) = delete;
        Decoder &&operator=(Decoder &&) = delete;

    protected:

        /**
         * Path to the video resource, i.e. "/dev/video0".
         */
        std::string _videoSourcePath;

        /**
         * Height of the captured frame.
         * This can be changed by the decoder.
         */
        size_t _frameHeight;

        /**
         * Width of the captured frame.
         * This can be changed by the decoder.
         */
        size_t _frameWidth;

        /*
         * Frames per second (FPS).
         * This can be changed by the decoder.
         */
        size_t _frameRate;

        /**
         * Number of bits per second that the video source can provide with.
         * This can be changed by the decoder.
         */
        size_t _bitRate;

        /**
         * Callback function.
         * Passes the captured frame's data with its size in bytes to the callee.
         */
        std::function<void(uint8_t *)> _onFrameCallback;
    };
}

#endif //LIVE_VIDEO_STREAM_DECODER_HPP
