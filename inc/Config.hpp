#ifndef LIVE_VIDEO_STREAM_CONFIG_HPP
#define LIVE_VIDEO_STREAM_CONFIG_HPP

#include <string>
#include "Logger.hpp"
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>

namespace po = boost::program_options;

#define RARE_CAM "/dev/v4l/by-path/pci-0000:00:14.0-usb-0:3:1.0-video-index0"
#define LEFT_CAM "/dev/v4l/by-path/pci-0000:00:14.0-usb-0:4:1.0-video-index0"
#define RIGHT_CAM "/dev/v4l/by-path/pci-0000:00:1a.0-usb-0:1.3:1.0-video-index0"
#define ZOOM_CAM "/dev/v4l/by-path/pci-0000:00:14.0-usb-0:2:1.0-video-index0"

namespace LIRS {

    class Configuration {

    public:

        constexpr static unsigned int DEFAULT_STREAMING_FRAMERATE = 5;

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

        bool loadConfig(int cliArgc, char **cliArgs) {

            po::options_description cli_options;

            po::options_description generic("Generic options");
            po::options_description streaming("Streaming configuration");
            po::options_description codec("Codec configuration");

            generic.add_options()
                    ("version,v", "print version")
                    ("help", "produce help message");

            streaming.add_options()
                    ("source,s", po::value<std::string>(&streaming_source_url)->default_value(RARE_CAM), "set video streaming camera url")
                    ("topic", po::value<std::string>(&streaming_topic_name)->default_value("rare"), "set streaming topic name")
                    ("width,w", po::value<size_t>(&origin_frame_width)->default_value(DEFAULT_FRAME_WIDTH), "set original frame width")
                    ("height,h", po::value<size_t>(&origin_frame_height)->default_value(DEFAULT_FRAME_HEIGHT), "set original frame height")
                    ("out-width", po::value<size_t>(&streaming_frame_width)->default_value(DEFAULT_FRAME_WIDTH), "set streaming frame width")
                    ("out-height", po::value<size_t>(&streaming_frame_height)->default_value(DEFAULT_FRAME_HEIGHT), "set streaming frame height")
                    ("pix-fmt,f", po::value<std::string>(&origin_pixel_format)->default_value("bayer_grbg"), "set original pixel format")
                    ("codec-pix-fmt", po::value<std::string>(&codec_input_pixel_format)->default_value("yuv420p"), "set codec pixel format")
                    ("fps-num", po::value<size_t>(&origin_framerate_num)->default_value(DEFAULT_STREAMING_FRAMERATE), "set original framerate numerator")
                    ("fps-den", po::value<size_t>(&origin_framerate_den)->default_value(1), "set original framerate denumerator")
                    ("out-fps-num", po::value<size_t>(&streaming_framerate_num)->default_value(DEFAULT_STREAMING_FRAMERATE), "set streaming framerate numerator")
                    ("out-fps-den", po::value<size_t>(&streaming_framerate_den)->default_value(1), "set streaming framerate denumerator");

            cli_options.add(generic).add(streaming);

            po::variables_map vm;

            po::store(po::parse_command_line(cliArgc, reinterpret_cast<const char *const *>(cliArgs),
                                             cli_options), vm, true);

            po::notify(vm);

            if (vm.count("help")) {
                std::cout << cli_options << std::endl;
                return false;
            }

            LOG(WARN) << streaming_topic_name << ", " << streaming_source_url;

            return true;
        }

    private:

        std::string streaming_source_url;

        std::string streaming_topic_name;

        size_t origin_frame_width;
        size_t streaming_frame_width;

        size_t origin_frame_height;
        size_t streaming_frame_height;

        std::string origin_pixel_format;
        std::string codec_input_pixel_format;

        size_t origin_framerate_num;
        size_t origin_framerate_den;

        size_t streaming_framerate_num;
        size_t streaming_framerate_den;

        // Codec settings

        size_t number_of_slices;
        size_t vbv_bufsize_bytes;
        bool intra_refresh_enabled;
        size_t udp_packet_size_bytes;
        size_t estimated_bitrate_kbps;
    };
}

#endif //LIVE_VIDEO_STREAM_CONFIG_H
