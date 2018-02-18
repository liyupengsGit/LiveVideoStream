#include <thread>
#include "Decoder.hpp"

namespace LIRS {

    const std::string Decoder::getVideoSourcePath() const {
        return _videoSourcePath;
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
}