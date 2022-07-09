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

#define INTERNALPERFKIT_LOCTEXT_FULL(HashStr, RefText, Label)                                \
    ([]() -> std::string const& {                                                            \
        static constexpr auto hash = cpph::hasher::fnv1a_64(HashStr);                        \
        static auto ctx = perfkit::detail::loca_create_static_context(hash, RefText, Label); \
        return perfkit::detail::loca_lookup(ctx);                                            \
    }())

#define PERFKIT_KEYTEXT(Label, RefText) INTERNALPERFKIT_LOCTEXT_FULL(#Label, RefText, #Label)
#define PERFKIT_KEYWORD(Label)          INTERNALPERFKIT_LOCTEXT_FULL(#Label, #Label, #Label)
#define PERFKIT_LOCTEXT(RefText)        INTERNALPERFKIT_LOCTEXT_FULL(RefText, RefText, nullptr)
#define PERFKIT_LOCWORD(RefText)        INTERNALPERFKIT_LOCTEXT_FULL(RefText, RefText, RefText)

#define PERFKIT_C_KEYTEXT(Label, RefText) INTERNALPERFKIT_LOCTEXT_FULL(#Label, RefText, #Label).c_str()
#define PERFKIT_C_KEYWORD(Label)          INTERNALPERFKIT_LOCTEXT_FULL(#Label, #Label, #Label).c_str()
#define PERFKIT_C_LOCTEXT(RefText)        INTERNALPERFKIT_LOCTEXT_FULL(RefText, RefText, nullptr).c_str()
#define PERFKIT_C_LOCWORD(RefText)        INTERNALPERFKIT_LOCTEXT_FULL(RefText, RefText, RefText).c_str()
