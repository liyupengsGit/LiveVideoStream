if (LIVE555_LIBRARIES AND LIVE555_INCLUDE_DIR)
    set(LIVE555_FOUND TRUE)
else(LIVE555_LIBRARIES AND LIVE555_INCLUDE_DIR)

    find_path(LIVE555_INCLUDE_DIR liveMedia/liveMedia.hh PATHS /usr/lib/live)

    set(LIVE555_LIBRARIES "")
    set(LIVE555_FOUND TRUE)

    foreach (LIBLIVE555_module groupsock liveMedia BasicUsageEnvironment UsageEnvironment )
        find_library( ${LIBLIVE555_module}_LIBRARY ${LIBLIVE555_module} )
        if ( ${LIBLIVE555_module}_LIBRARY )
            message("Found " ${LIBLIVE555_module} ": " ${${LIBLIVE555_module}_LIBRARY})
            set(LIVE555_LIBRARIES ${LIVE555_LIBRARIES} ${${LIBLIVE555_module}_LIBRARY})
        endif ( ${LIBLIVE555_module}_LIBRARY )

        if (NOT ${LIBLIVE555_module}_LIBRARY)
            set(LIVE555_FOUND FALSE)
        endif(NOT ${LIBLIVE555_module}_LIBRARY)

    endforeach (LIBLIVE555_module)

endif(LIVE555_LIBRARIES AND LIVE555_INCLUDE_DIR)