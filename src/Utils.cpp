#include "Utils.hpp"
#include <sstream>

namespace LIRS {

    namespace utils {

        std::string concatParams(std::initializer_list<size_t> args, std::string delimiter, std::string tail) {
            std::stringstream resultStream;
            for (auto it = args.begin(); it != (args.end() - 1); ++it) { // iterate over all except last element
                resultStream << (*it) << delimiter;
            }
            resultStream << *(args.end() - 1); // add last element w/o delimiter
            resultStream << tail;
            return resultStream.str();
        }
    }
}
