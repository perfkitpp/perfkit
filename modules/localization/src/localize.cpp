#include "perfkit/localize.h"

#include <cpph/std/algorithm>
#include <cpph/std/map>
#include <cpph/std/unordered_map>
#include <fstream>
#include <shared_mutex>

#include "cpph/algorithm/base64.hxx"
#include "cpph/refl/object.hxx"
#include "cpph/refl/types/tuple.hxx"
#include "cpph/refl/types/unordered.hxx"
#include "cpph/thread/event_wait.hxx"
#include "cpph/thread/locked.hxx"
#include "cpph/thread/thread_pool.hxx"
#include "cpph/utility/singleton.hxx"

//

namespace perfkit {
namespace detail {

struct loca_text_entity {
    CPPH_REFL_DEFINE_OBJECT_inline_simple(label, content);

    string label;
    string content;
};

using loca_full_lut_t = unordered_map<uint64_t, loca_text_entity>;
using loca_mutable_lut_t = unordered_map<uint64_t, string>;
using loca_lut_t = const unordered_map<uint64_t, string>;

static auto acquire_global_builder() { return default_singleton<locked<loca_full_lut_t>, class TMP_0>().lock(); }

static auto acquire_loaded_tables() { return default_singleton<locked<map<string, loca_lut_t*, std::less<>>>>().lock(); }

static atomic<loca_lut_t*> loca_active_lut_table = nullptr;

struct loca_static_context {
    uint64_t hash;
    string_view ref_text;
};

static string const* lookup(uint64_t hash)
{
    if (auto lut = relaxed(loca_active_lut_table))
        if (auto pair = find_ptr(*lut, hash))
            return &pair->second;

    return nullptr;
}

static void send_string_to_builder(uint64_t hash, string_view refstr, string_view label)
{
    char base64_label[base64::encoded_size(sizeof hash)];
    if (label.empty()) {
        base64::encode_bytes(&hash, sizeof hash, base64_label);
        label = {base64_label, sizeof base64_label};
    }

    auto builder = acquire_global_builder();
    auto [iter, is_first] = builder->try_emplace(hash);

    if (is_first) {
        iter->second.content = refstr;
        iter->second.label = label;
    }
}

ptr<loca_static_context> loca_create_static_context(uint64_t hash, const char* ref_text, const char* label) noexcept
{
    ptr<loca_static_context> ctx{new loca_static_context{hash, ref_text}};

    if (not lookup(hash)) {
        send_string_to_builder(hash, ref_text, label ? label : string_view{});
    }

    return ctx;
}

string_view loca_lookup(loca_static_context* ctx) noexcept
{
    if (auto text = lookup(ctx->hash))
        return *text;
    else
        return ctx->ref_text;
}

}  // namespace detail

localization_load_result load_localization_lut(string_view key, archive::if_reader* strm)
{
    // Load struct
    detail::loca_full_lut_t table;
    try {
        *strm >> table;
    } catch (archive::error::reader_exception&) {
        return localization_load_result::invalid_content;
    }

    // Build LUT
    auto lut = make_unique<detail::loca_mutable_lut_t>();
    lut->reserve(table.size());
    for (auto& [hash, pair] : table) {
        lut->try_emplace(hash, move(pair.content));
    }

    // Check if table with same-key is already loaded ... As language tables are not unloaded,
    //  not any language table with same key is permitted to be loaded multiple times.
    {
        auto globals = detail::acquire_loaded_tables();

        if (auto existing_table = find_ptr(*globals, key)) {
            release(detail::loca_active_lut_table, existing_table->second);
            return localization_load_result::already_loaded;
        } else {
            globals->try_emplace(string{key}, lut.get());
        }
    }

    // Send text to global builder
    {
        auto builder = detail::acquire_global_builder();
        builder->merge(move(table));
    }

    // Replace global LUT
    release(detail::loca_active_lut_table, lut.release());
    return localization_load_result::okay;
}

void dump_localization_lut(archive::if_writer* strm)
{
    auto table = *detail::acquire_global_builder();
    *strm << table;
}
}  // namespace perfkit
