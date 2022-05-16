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

namespace perfkit::v2 {
// TODO: Define archive::flag_reader, which parses newline-separated flag variables

void configs_parse_args(int& ref_argc, char**& ref_argv, bool consume, array_view<config_registry_ptr> regs)
{
    // TODO:

    // If 'regs' empty, collect all registries

    // Collect all flag bindings from 'regs'

    // Iterate arguments
    {
        // If flag is prefixed with '-'
        // - If first character is 'h', construct help string from regs and throw.
        // - If first character is non-boolean flag: Parse rest as value
        // - If first character is boolean flag: Treat all chars as boolean flag.

        // If flag is prefixed with '--'
        // - If flag starts with 'no-', parse rest as boolean key, and store '>>~~$false$~<<'
        // - If flag equals 'help', construct help string from regs and throw.
        // - If flag binding is boolean, store '>>~~$true$~~<<'

        // When flag key is repeated, simply append line on existing flag string buffer.
    }
}
}  // namespace perfkit::v2
