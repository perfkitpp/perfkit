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

enum class localization_load_result {
    okay,
    invalid_file_path,
    invalid_content,
    already_loaded
};

localization_load_result load_localization_lut(string_view key, archive::if_reader*);
localization_load_result load_localization_lut(string_view key, string_view path);
void dump_localization_lut(archive::if_writer*);
void dump_localization_lut(string_view path);

}  // namespace perfkit

namespace perfkit::detail {
struct loca_static_context;

ptr<loca_static_context> loca_create_static_context(uint64_t hash, char const* ref_text, char const* label = nullptr) noexcept;
string_view loca_lookup(loca_static_context*) noexcept;
}  // namespace perfkit::detail

#define PERFKIT_KEYTEXT(Label, RefText)                                      \
    ([]() {                                                                  \
        static constexpr auto hash = cpph::hasher::fnv1a_64(#Label);         \
        static auto ctx = loca_create_static_context(hash, RefText, #Label); \
        return perfkit::detail::localize_lookup(ctx.get());                  \
    }())

#define PERFKIT_LOCTEXT(RefText)                                      \
    ([]() {                                                           \
        static constexpr auto hash = cpph::hasher::fnv1a_64(RefText); \
        static auto ctx = loca_create_static_context();               \
        return perfkit::detail::localize_lookup(ctx.get());           \
    }())
