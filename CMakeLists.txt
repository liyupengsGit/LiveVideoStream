cmake_minimum_required(VERSION 3.9)
project(LiveVideoStream)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_CXX_FLAGS "-Wall -pthread")
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)

find_package(OpenCV 3 REQUIRED)
find_package(FFmpeg REQUIRED)
find_package(Live555 REQUIRED)
find_package(Threads REQUIRED)

# executables
include_directories("inc")
add_executable(${PROJECT_NAME} ${SOURCE_FILES}
        src/logger.cpp src/main.cpp src/Decoder.cpp src/FFmpegDecoder.cpp src/Utils.cpp)

# pthread
target_link_libraries (${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

# opencv
include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})

# ffmpeg
if (FFMPEG_FOUND)
    include_directories(${FFMPEG_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${FFMPEG_LIBRARIES})
else(FFMPEG_FOUND)
    message(FATAL_ERROR "Can't find FFmpeg libs libavcodec, libavformat or libavutil.")
endif (FFMPEG_FOUND)

# LOG4CPP
find_path(LOG4CPP_INCLUDE_DIR log4cpp/Category.hh)
if (LOG4CPP_INCLUDE_DIR)
    message(STATUS "Log4Cpp available")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_LOG4CPP")
    include_directories(${LOG4CPP_INCLUDE_DIR})

    find_library(LOG4CPP_LIBRARY log4cpp)
    target_link_libraries(${PROJECT_NAME} "${LOG4CPP_LIBRARY}")
endif (LOG4CPP_INCLUDE_DIR)

# live555
if (LIVE555_FOUND)
    message("Found Live555 libraries")

    foreach(LIBLIVE555_module liveMedia groupsock UsageEnvironment BasicUsageEnvironment)
        include_directories(${LIVE555_INCLUDE_DIR}/${LIBLIVE555_module})
    endforeach(LIBLIVE555_module)

    target_link_libraries(${PROJECT_NAME} ${LIVE555_LIBRARIES})

else(LIVE555_FOUND)
    message("Can't find Live555 libraries")
endif(LIVE555_FOUND)