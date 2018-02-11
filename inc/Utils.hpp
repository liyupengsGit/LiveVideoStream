#ifndef LIVEVIDEOSTREAM_UTILS_H
#define LIVEVIDEOSTREAM_UTILS_H

#include <string>
#include <initializer_list>

namespace LIRS {

    /**
     * Utility methods.
     */

    namespace utils {

        /**
         * Concatenates specified arguments using delimiter/tail.
         *
         * @param args elements to be concatenated.
         * @param delimiter string used between concatenated arguments.
         * @param tail string concatenated at the end.
         * @return resulting concatenated string.
         */
        std::string concatParams(std::initializer_list<size_t> args, std::string delimiter = {}, std::string tail = {});
    }
}

#endif //LIVEVIDEOSTREAM_UTILS_H
