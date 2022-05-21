/*******************************************************************************
 * MIT License
 *
 * Copyright (c) 2022. Seungwoo Kang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * project home: https://github.com/perfkitpp
 ******************************************************************************/

#include "perfkit/detail/configs-v2-flags.hpp"

#include <sstream>

#include "cpph/refl/core.hxx"
#include "cpph/refl/detail/if_archive.hxx"
#include "perfkit/detail/configs-v2-backend.hpp"

namespace perfkit::v2 {
// TODO: Define archive::flag_reader, which parses newline-separated flag variables
class flag_reader : public archive::if_reader
{
   public:
    explicit flag_reader(std::streambuf* buf) noexcept : if_reader(buf) {}

   public:
    if_reader& read(nullptr_t a_nullptr) override
    {
        // TODO
        return *this;
    }
    if_reader& read(bool& v) override
    {
        // TODO
        return if_reader::read(v);
    }
    if_reader& read(int64_t& v) override
    {
        // TODO
        return *this;
    }
    if_reader& read(double& v) override
    {
        // TODO
        return *this;
    }
    if_reader& read(string& v) override
    {
        // TODO
        return *this;
    }

    archive::context_key begin_object() override { _throw_no_support(); }
    void end_array(archive::context_key key) override { _throw_no_support(); }
    void read_key_next() override { _throw_no_support(); }
    void end_object(archive::context_key key) override { _throw_no_support(); }
    size_t begin_binary() override { _throw_no_support(); }
    size_t binary_read_some(mutable_buffer_view v) override { _throw_no_support(); }
    void end_binary() override { _throw_no_support(); }

    archive::context_key begin_array() override
    {
        return {1};
    }

    bool should_break(const archive::context_key& key) const override
    {
        return _buf->sgetc() == EOF;
    }

    archive::entity_type type_next() const override
    {
        return archive::entity_type::string;
    }

   private:
    [[noreturn]] void _throw_no_support()
    {
        throw std::logic_error{"Flag must be bound to {boolean|number|string|array<boolean|number|string>}"};
    }
};

// Meaningless character sequence to identify internal flag value
#define FLAG_ESCAPE   "$c`|*;2^n0"
#define FLAG_ESCAPE_N (sizeof(FLAG_ESCAPE) - 1)

void configs_parse_args(int& ref_argc, char const**& ref_argv, config_parse_arg_option option, array_view<config_registry_ptr> regs)
{
    // If 'regs' empty, collect all registries
    vector<config_registry_ptr> opt_all_regs;

    if (regs.empty()) {
        config_registry::backend_t::enumerate_registries(&opt_all_regs, option.collect_unregistered_registerise);
        regs = opt_all_regs;

        // Performs initial update on registries, to make all pending flags to be registered.
        if (option.update_collected_registries)
            for (auto& rg : regs) { rg->update(); }
    }

    // Collect all flag bindings from 'regs'
    map<string_view, pair<config_registry*, config_base_ptr>> flag_bindings;
    map<config_base*, pair<config_registry*, std::stringbuf>> flag_payloads;

    vector<config_base_ptr> all_items;
    for (auto& rg : regs) {
        all_items.clear();
        rg->backend()->bk_all_items(&all_items);

        for (auto& item : all_items) {
            auto& bindings = item->attribute()->flag_bindings;
            if (bindings.empty()) { continue; }

            for (auto& binding : bindings) {
                if (
                        not flag_bindings.try_emplace(binding, pair{rg.get(), item}).second
                        &&                                 //
                        not option.allow_flag_duplication  //
                ) {
                    throw std::logic_error{"Flag binding duplication!"};
                }
            }
        }
    }

    // Helpers
    auto fn_is_w = [](auto c) { return isalnum(c) || c == '-' || c == '_'; };

    // Iterate arguments
    for (char const **p_arg = ref_argv + int(option.is_first_arg_exec_name),
                    **p_end = ref_argv + ref_argc;
         p_arg < p_end;) {
        string_view arg = *p_arg;

        if (arg == "--")
            break;  // Stop parsing on '--'

        if (arg[0] == '-' && arg[1] == '-' && arg[2] != '-') {
            // If flag is prefixed with '--'
            string_view key = arg.substr(2);  // after '--'
            string_view value;

            auto dividx = find_if_not(key, fn_is_w) - key.begin();
            value = key.substr(dividx);
            key = key.substr(0, dividx);

            auto binding = find_ptr(flag_bindings, key);
            if (not binding) {
                if (not option.allow_unknown_flag)
                    throw std::runtime_error{"Unknown flag: " + string{key}};

                continue;
            }

            // Retrieve type metadata
            auto& [reg, cfg] = binding->second;
            auto metadata = cfg->attribute()->default_value.view().meta;

            if (key.find("no-") == 0) {
                // - If flag starts with 'no-', parse rest as boolean key, and store '>>~~$false$~<<'
                // TODO
            } else if (key == "help") {
                // - If flag equals 'help', construct help string from regs and throw.
                // TODO
            } else if (metadata->type() == archive::entity_type::boolean) {
                // - If flag binding is boolean, store '>>~~$true$~~<<'
                auto& [rg, payload] = flag_payloads[cfg.get()];
                rg = reg, payload.sputn(FLAG_ESCAPE "true", FLAG_ESCAPE_N + 4);
            } else {
                // - It's just value ...
                //   If value is already found(was single argument), just use it.
                //   Otherwise, step p_arg. In this case, next p_arg *must* not be flag!
                // TODO
            }
        } else if (arg[0] == '-' && arg[1] != '-') {
            // If flag is prefixed with '-'
            // - If first character is 'h', construct help string from regs and throw.
            // - If first character is non-boolean flag: Parse rest as value
            // - If first character is boolean flag: Treat all chars as boolean flag.
            // TODO:
        } else {
            continue;
        }
    }

    // Perform actual parsing
    for (auto& [cfg, pair] : flag_payloads) {
        auto& [reg, payload] = pair;

        flag_reader rd{&payload};
        if (not reg->backend()->bk_commit(cfg, &rd)) {
            // TODO: Throw error string
        }
    }

    if (option.consume_flags) {
        // Erase all consumed flags from argv
        array_view argv{ref_argv, size_t(ref_argc)};
        auto iter = stable_partition(argv, [](auto p) { return p != nullptr; });
        ref_argc = iter - argv.begin();
    }
}
}  // namespace perfkit::v2
