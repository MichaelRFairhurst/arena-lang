#ifndef ARENA_INCLUDE_ERRORS_LINE_ENDINGS_HPP
#define ARENA_INCLUDE_ERRORS_LINE_ENDINGS_HPP

#include <cstddef>
#include <vector>
#include <optional>
#include <string_view>

namespace arena::error {
    struct Cursor {
        size_t line;
        size_t column;
    };

    struct Range {
        Cursor start;
        Cursor end;
    };

    class LineEndings {
        public:
        LineEndings() = default;
        LineEndings(std::string_view source);

        Cursor offset_to_cursor(size_t offset) const;
        Range offset_length_to_range(size_t offset, size_t length) const;
        std::optional<size_t> line_offset(size_t line) const;
        std::string_view get_line(std::string_view source, size_t line) const;


        bool operator==(const LineEndings &other) const {
            return line_start_offsets == other.line_start_offsets;
        }
        
        bool operator!=(const LineEndings &other) const { return !(*this == other); }

        private:
        std::vector<size_t> line_start_offsets;
    };

} // namespace arena::error

#endif // ARENA_INCLUDE_ERRORS_LINE_ENDINGS_HPP