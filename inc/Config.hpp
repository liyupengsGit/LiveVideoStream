#ifndef LIVE_VIDEO_STREAM_CONFIG_HPP
#define LIVE_VIDEO_STREAM_CONFIG_HPP

#include <string>

#define RARE_CAM "/dev/v4l/by-path/pci-0000:00:14.0-usb-0:3:1.0-video-index0"
#define LEFT_CAM "/dev/v4l/by-path/pci-0000:00:14.0-usb-0:4:1.0-video-index0"
#define RIGHT_CAM "/dev/v4l/by-path/pci-0000:00:1a.0-usb-0:1.3:1.0-video-index0"
#define ZOOM_CAM "/dev/v4l/by-path/pci-0000:00:14.0-usb-0:2:1.0-video-index0"

namespace LIRS {

    struct Configuration {

    private:

        static uint framerate;
        static uint frame_width;
        static uint frame_height;
        static uint udp_packet_size_bytes;
        static uint estimated_bitrate_kbps;
        static uint vbv_bufsize_bytes;
        static short intra_refresh_enabled;
        static short slices;

    public:

        constexpr static unsigned int DEFAULT_FRAMERATE = 5;

        constexpr static unsigned int DEFAULT_FRAME_WIDTH = 744;

        constexpr static unsigned int DEFAULT_FRAME_HEIGHT = 480;

        constexpr static unsigned int DEFAULT_UDP_PACKET_SIZE = 1500;

        constexpr static unsigned int DEFAULT_BITRATE_KBPS = 200;

        // =========== Codec settings ================= //

        constexpr static unsigned int DEFAULT_VBV_BUFSIZE = 512;

        constexpr static unsigned int DEFAULT_INTRA_REFRESH_ENABLED = 0;

        constexpr static unsigned int DEFAULT_NUM_SLICES = 1;

        constexpr static char const *CODEC_PARAMS_PATTERN = "slices=%d:intra-refresh=%d:vbv-maxrate=%d:vbv-bufsize=%d";

        static std::string get_default_codec_params_str() {
            char formattedStr[512];
            sprintf(formattedStr, CODEC_PARAMS_PATTERN, DEFAULT_NUM_SLICES, DEFAULT_INTRA_REFRESH_ENABLED,
                    DEFAULT_BITRATE_KBPS, DEFAULT_VBV_BUFSIZE);
            return std::string(formattedStr);
        }

    };
}

#endif //LIVE_VIDEO_STREAM_CONFIG_H
