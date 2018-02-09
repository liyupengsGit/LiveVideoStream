#ifndef LIVE_VIDEO_STREAM_DECODER_H
#define LIVE_VIDEO_STREAM_DECODER_H

#include <string>
#include <functional>
#include <utility>
#include "logger.h"

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
     * Abstract class that captures frames from the specified video source.
     */
    class Decoder {
    public:

        /**
         * Creates decoder with the specified video source and parameters.
         * Required constructor.
         *
         * @param videoSourcePath path to the video source, i.e. "/dev/video0"
         * @param params parameters of the decoder, i.e. frame weight and height
         */
        explicit Decoder(std::string videoSourcePath, DecoderParams params = {}) :
                _videoSourcePath(std::move(videoSourcePath)),
                _frameHeight(params.frameHeight), _frameWidth(params.frameWidth),
                _frameRate(params.frameRate), _bitRate(0), _gop(0) {}

        /**
         * Sets the callback function that will be invoked
         * when a frame from the video source was captured.
         *
         * @param callback the function to be invoked when a frame was captured.
         */
        void setOnFrameCallback(std::function<void(uint8_t *)> callback) {
            this->_onFrameCallback = std::move(callback);
        }

        /**
         * Loop capturing video frames and invoking the specified callback.
         */
        virtual void decodeLoop() = 0;

        // getters

        const std::string &get_videoSourcePath() const {
            return _videoSourcePath;
        }

        size_t get_frameHeight() const {
            return _frameHeight;
        }

        size_t get_frameWidth() const {
            return _frameWidth;
        }

        size_t get_frameRate() const {
            return _frameRate;
        }

        size_t get_bitRate() const {
            return _bitRate;
        }

        size_t get_gop() const {
            return _gop;
        }

        // end getters

        /**
         * Nothing to say, it's simply destructor.
         */
        virtual ~Decoder() = default;

        // restrict move & copy constructor and copy assignment operator
        Decoder(Decoder &&) = delete;

        Decoder(const Decoder &) = delete;

        Decoder &operator=(const Decoder &) = delete;

        Decoder &&operator=(Decoder &&) = delete;


    protected:

        // path to the video resource
        std::string _videoSourcePath;

        // frame's dimensions
        size_t _frameHeight;
        size_t _frameWidth;

        // rates
        size_t _frameRate;
        size_t _bitRate;
        size_t _gop;

        // callback function, triggers when a new frame is captured
        std::function<void(uint8_t *)> _onFrameCallback;
    };
}

#endif //LIVE_VIDEO_STREAM_DECODER_H
