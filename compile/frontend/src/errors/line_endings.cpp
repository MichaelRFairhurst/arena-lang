#include "errors/line_endings.hpp"

#include <algorithm>
using namespace arena::error;

LineEndings::LineEndings(std::string_view source) {
    auto begin = source.begin();
    auto it = source.begin();
    while (it != source.end()) {
        line_start_offsets.push_back(std::distance(source.begin(), it));
        it = std::find(it, source.end(), '\n');
        if (it != source.end()) {
            ++it; // move past the newline for the next line start
        }
    }
}

Cursor LineEndings::offset_to_cursor(size_t offset) const {
    auto it = std::lower_bound(line_start_offsets.begin(), line_start_offsets.end(), offset);
    auto line_index = std::distance(line_start_offsets.begin(), it) - 1;
    auto column = offset - line_start_offsets[line_index];
    return Cursor{static_cast<size_t>(line_index), column};
}

Range LineEndings::offset_length_to_range(size_t offset, size_t length) const {
    auto start = offset_to_cursor(offset);
    auto end = offset_to_cursor(offset + length);
    return Range{start, end};
}

std::optional<size_t> LineEndings::line_offset(size_t line) const {
    if (line >= line_start_offsets.size()) {
        return std::nullopt;
    }
    return line_start_offsets[line];
}

std::string_view LineEndings::get_line(std::string_view source, size_t line) const {
    auto line_start_offset = line_offset(line).value_or(source.size());
    // Next line, minus the newline character, or end of source for the last line.
    auto line_end_offset = line_offset(line + 1).value_or(source.size() + 1) - 1;
    if (line_start_offset > source.size() || line_end_offset > source.size()) {
        throw std::runtime_error("Line number is out of bounds");
    }

    return source.substr(line_start_offset, line_end_offset - line_start_offset);
}