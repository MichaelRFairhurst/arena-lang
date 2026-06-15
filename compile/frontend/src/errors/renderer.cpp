#include "errors/renderer.hpp"
#include "errors/line_endings.hpp"
#include "query/queries.hpp"
#include "query/engine.hpp"

using namespace arena::error;
using namespace arena;

namespace {
    struct Source {
        std::string_view contents;
        std::filesystem::path path;
        LineEndings line_endings;
    };

    class RenderImpl {
    public:
        size_t start_offset_of(const Source &source, const ast::Token *token) const {
            auto contents = source.contents;
            auto contents_addr_start = contents.data();
            auto token_addr_start = token->text.data();

            return std::distance(contents_addr_start, token_addr_start);
        }

        size_t end_offset_of(const Source &source, const ast::Token *token) const {
            auto contents = source.contents;
            auto contents_addr_start = contents.data();
            auto token_addr_end = token->text.data() + token->text.size();

            return std::distance(contents_addr_start, token_addr_end);
        }

        std::string location_string(const Source &source, const Cursor &cursor) const {
            return source.path.string() + ":" + std::to_string(cursor.line + 1) + ":" +
                   std::to_string(cursor.column + 1);
        }

        std::string location_string(const Source &source, const Range &range) const {
            return source.path.string() + ":" + std::to_string(range.start.line + 1) + ":" +
                   std::to_string(range.start.column + 1) + ":" +
                   std::to_string(range.end.line + 1) + ":" + std::to_string(range.end.column + 1);
        }

        std::string cursor_range(size_t start, size_t end, char marker) const {
            std::string result;
            for (size_t i = 0; i < start; ++i) {
                result += " ";
            }
            for (size_t i = start; i < end; ++i) {
                result += marker;
            }
            return result;
        }

        std::string show_multiline_subrange(const Source &source,
                                            const Range &range,
                                            std::string message) const {
            size_t line = range.start.line;
            std::string_view line_text = source.line_endings.get_line(source.contents, line);
            std::string result =
                "  : " + cursor_range(range.start.column, line_text.size(), 'v') + "\n";
            result += "  : " + std::string(line_text) + "\n";
            line++;
            if (range.end.line > line) {
                result +=
                    "  :  ... (" + std::to_string(range.end.line - line) + " more lines) ...\n";
            }
            line = range.end.line;
            result +=
                "  : " + std::string(source.line_endings.get_line(source.contents, line)) + "\n";
            result += "  : " + cursor_range(0, range.end.column, '^') + " " + message + "\n";

            return result;
        }

        std::string show_line_subrange(const Source &source,
                                       std::string message,
                                       size_t line,
                                       size_t column_start,
                                       size_t column_end) const {
            std::string result =
                "  : " + std::string(source.line_endings.get_line(source.contents, line));
            result += "\n  : ";
            result += cursor_range(column_start, column_end, '^') + " " + message + "\n";

            return result;
        }

        std::string show_range(const Source &source,
                               const error::Range &range,
                               std::string message) const {
            std::string result;
            if (range.start.line == range.end.line) {
                result += show_line_subrange(source,
                                             message,
                                             range.start.line,
                                             range.start.column,
                                             range.end.column);
            } else {
                result += show_multiline_subrange(source, range, message);
            }

            return result;
        }

        std::string show_cause(const Source &source, const error::Cause &cause) const {
            if (!cause.location.has_value()) {
                return "  : " + std::string(cause.description) + "\n";
            }
            auto begin = start_offset_of(source, cause.location->begin);
            auto end = end_offset_of(source, cause.location->end);

            auto range = source.line_endings.offset_length_to_range(begin, end - begin);
            std::string prefix = location_string(source, range.start) + "\n";
            return prefix + show_range(source, range, std::string(cause.description));
        }

        std::string show_link(const Source &source, const error::Link &link) const {
            auto begin = start_offset_of(source, link.location.begin);
            auto end = end_offset_of(source, link.location.end);

            auto range = source.line_endings.offset_length_to_range(begin, end - begin);
            std::string prefix = location_string(source, range.start) + "\n";
            return prefix + show_range(source, range, std::string(link.label_text));
        }

        std::string error_message(const error::Error &error) const {
            std::string result;
            for (const auto &chunk : error.get_chunks()) {
                if (auto str = std::get_if<std::string>(&chunk)) {
                    result += *str;
                } else if (auto str_view = std::get_if<std::string_view>(&chunk)) {
                    result += *str_view;
                } else if (auto cstr = std::get_if<const char *>(&chunk)) {
                    result += *cstr;
                } else if (auto link = std::get_if<error::Link>(&chunk)) {
                    result += link->link_text;
                } else {
                    throw std::runtime_error("Unknown chunk type in error message");
                }
            }
            return result;
        }

        std::string supplement_message(const Source &source, const error::Supplement &supplement) const {
            std::string result = "  .\n";
            switch (supplement.kind) {
            case error::SupplementKind::Note:
                result += " == Note: ";
                break;
            case error::SupplementKind::Help:
                result += " == Help: ";
                break;
            }
            result += supplement.message;

            if (supplement.location.has_value()) {
                auto begin = start_offset_of(source, supplement.location->begin);
                auto end = end_offset_of(source, supplement.location->end);

                auto range = source.line_endings.offset_length_to_range(begin, end - begin);
                std::string location = location_string(source, range.start);
                result += "\n" + show_range(source, range, " --- " + location);
            }
            return result;
        }
    };
} // namespace

std::string CliRenderer::render(std::filesystem::path path,
                                const std::vector<Error> &errors,
                                const sema::QueryEngineContext &context) {
    RenderImpl impl;

    std::string result;
    for (const auto &error : errors) {
        Source source{context.run_query(sema::SourceContentsQuery{path}),
                      path,
                      context.run_query(sema::LineEndingsQuery{path})};

        std::string rendered_error = "\n";
        auto error_start = impl.start_offset_of(source, error.get_begin());
        auto error_end = impl.end_offset_of(source, error.get_end());
        auto error_range =
            source.line_endings.offset_length_to_range(error_start, error_end - error_start);

        rendered_error +=
            "Error[" + std::string(error.get_code()) + "]: " + impl.error_message(error) + "\n";
        rendered_error += "at " + impl.location_string(source, error_range) + "\n";
        rendered_error += impl.show_range(source, error_range, std::string(error.get_label()));

        for (const auto chunk : error.get_chunks()) {
            if (auto link = std::get_if<error::Link>(&chunk)) {
                rendered_error += impl.show_link(source, *link);
            }
        }

        for (const auto &supplement : error.get_supplements()) {
            rendered_error += impl.supplement_message(source, supplement);
        }

        for (const auto &cause : error.get_causes()) {
            rendered_error += "  .\n";
            rendered_error += " ... caused by\n";
            rendered_error += "  -> " + impl.show_cause(source, cause);
        }

        result += rendered_error + "\n";
    }

    return result;
}