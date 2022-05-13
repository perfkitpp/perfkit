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

#pragma once
#include "cpph/macros.hxx"
#include "detail/configs-v2-flags.hpp"
#include "detail/configs-v2.hpp"

#define INTL_PERFKIT_NS_0 ::perfkit::v2
#define INTL_PERFKIT_NS_1 ::perfkit::v2::_configs

#define PERFKIT_CFG(ClassName) class ClassName : public INTL_PERFKIT_NS_1::config_set<ClassName>

/**
 * Defines subset
 */
#define PERFKIT_CFG_SUBSET(ClassName, InstanceName, ...)

/**
 * Default config item definition macro
 */
#define PERFKIT_CFG_ITEM(VarName, DefaultValue, UserKey /*={}*/, ... /*AttributeDecorators*/)

/**
 * For legacy compatibility ... This defines attribute variable as static inline, which causes
 *  overhead on uninitialized
 */
#define PERFKIT_CFG_LEGACY_ITEM(VarName, ...)
