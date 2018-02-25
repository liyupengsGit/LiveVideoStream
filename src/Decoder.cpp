#include <thread>
#include <utility>
#include "Decoder.hpp"

namespace LIRS {

    const std::string &Decoder::getVideoSourcePath() const {
        return _videoSourcePath;
    }

    const std::string &Decoder::getPixelFormat() const {
        return _pixelFormat;
    }

    size_t Decoder::getFrameHeight() const {
        return _frameHeight;
    }

    size_t Decoder::getFrameWidth() const {
        return _frameWidth;
    }

    size_t Decoder::getFrameRate() const {
        return _frameRate;
    }

    size_t Decoder::getBitRate() const {
        return _bitRate;
    }

    void Decoder::setOnFrameCallback(const std::function<void(uint8_t *)> &onFrameCallback) {
        _onFrameCallback = onFrameCallback;
    }

    Decoder::Decoder(std::string videoSourcePath, DecoderParams params) :
            _videoSourcePath(std::move(videoSourcePath)), _pixelFormat(params.pixelFormat),
            _frameHeight(params.frameHeight), _frameWidth(params.frameWidth),
            _frameRate(params.frameRate), _bitRate(0) {}

    Decoder::~Decoder() {
        _videoSourcePath.clear();
        _pixelFormat.clear();
        _frameHeight = 0;
        _frameWidth = 0;
        _frameRate = 0;
        _bitRate = 0;
        _onFrameCallback = {}; // fixme: dunno how to properly reset/clear function
    }


}