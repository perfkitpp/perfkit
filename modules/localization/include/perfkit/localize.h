#pragma once
#include <cpph/std/memory>
#include <cpph/std/string>

#include <cpph/utility/hasher.hxx>

namespace cpph::archive {
class if_writer;
class if_reader;
}  // namespace cpph::archive

namespace perfkit {
using namespace cpph;

enum class localization_result {
    okay,
    invalid_file_path,
    invalid_content,
    already_loaded,
    key_not_loaded,
};

localization_result load_localization_lut(string_view key, archive::if_reader*);
localization_result load_localization_lut(string_view key, string const& path);
localization_result select_locale(string_view key);
localization_result dump_localization_lut(archive::if_writer*);
localization_result dump_localization_lut(string_view path);

}  // namespace perfkit

namespace perfkit::detail {
struct loca_static_context;

loca_static_context* loca_create_static_context(uint64_t hash, char const* ref_text, char const* label) noexcept;
string const& loca_lookup(loca_static_context*) noexcept;
}  // namespace perfkit::detail

#define PERFKIT_KEYTEXT(Label, RefText)                                                       \
    ([]() -> std::string const& {                                                             \
        static constexpr auto hash = cpph::hasher::fnv1a_64(#Label);                          \
        static auto ctx = perfkit::detail::loca_create_static_context(hash, RefText, #Label); \
        return perfkit::detail::loca_lookup(ctx);                                             \
    }())

#define PERFKIT_KEYWORD(Label)                                                               \
    ([]() -> std::string const& {                                                            \
        static constexpr auto hash = cpph::hasher::fnv1a_64(#Label);                         \
        static auto ctx = perfkit::detail::loca_create_static_context(hash, #Label, #Label); \
        return perfkit::detail::loca_lookup(ctx);                                            \
    }())

#define PERFKIT_LOCTEXT(RefText)                                                               \
    ([]() -> std::string const& {                                                              \
        static constexpr auto hash = cpph::hasher::fnv1a_64(RefText);                          \
        static auto ctx = perfkit::detail::loca_create_static_context(hash, RefText, nullptr); \
        return perfkit::detail::loca_lookup(ctx);                                              \
    }())

#define PERFKIT_LOCWORD(RefText)                                                               \
    ([]() -> std::string const& {                                                              \
        static constexpr auto hash = cpph::hasher::fnv1a_64(RefText);                          \
        static auto ctx = perfkit::detail::loca_create_static_context(hash, RefText, RefText); \
        return perfkit::detail::loca_lookup(ctx);                                              \
    }())
