# LiveVideoStream
C++ live camera video streaming server based on **Live555** framework. **FFmpeg API** is employed to decode/encode raw video data (frames) coming from the USB cameras in real-time.\
This server consists of the following parts/modules:
* `Transcoder` - captures raw video data from the camera and performs encoding using the specified codec (i. e. HEVC, or H.264);
*  `FramedSource` - serves as a layer between the server's video data source and encoded video data from the camera.
