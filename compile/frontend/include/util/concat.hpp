#ifndef ARENA_INCLUDE_UTIL_CONCAT_HPP
#define ARENA_INCLUDE_UTIL_CONCAT_HPP

#include <string>

namespace arena::util {

    template <typename It, typename Separator>
    std::string concat(It begin, It end, const Separator &separator) {
        std::string result;
        for (auto it = begin; it != end; ++it) {
            if (it != begin) {
                result += separator;
            }
            if constexpr (std::is_convertible_v<decltype(*it), std::string>) {
                result += *it;
            } else {
                // TODO: handle more types, and std::to_string, etc
                result += (*it)->to_string();
            }
        }
        return result;
    }

    template <typename Container, typename Separator>
    std::string concat(const Container &container, const Separator &separator) {
        return concat(container.begin(), container.end(), separator);
    }
} // namespace arena::util

#endif