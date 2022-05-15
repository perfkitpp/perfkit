// MIT License
//
// Copyright (c) 2021-2022. Seungwoo Kang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// project home: https://github.com/perfkitpp

#pragma once
#include "configs-v2.h"
#include "cpph/refl/object.hxx"
#include "cpph/refl/types/array.hxx"
#include "cpph/refl/types/list.hxx"
#include "cpph/refl/types/nlohmann-json.hxx"
#include "cpph/refl/types/tuple.hxx"
#include "cpph/refl/types/unordered.hxx"
#include "perfkit/fwd.hpp"

// HELPER MACROS
#define PERFKIT_CATEGORY(Category)      \
    PERFKIT_GCFG_CAT_ROOT_def(Category) \
    {                                   \
        bool update()                   \
        {                               \
            return registry().update(); \
        }                               \
    }                                   \
    namespace Category

#define PERFKIT_SUBCATEGORY(Category)   \
    PERFKIT_GCFG_CAT_def(Category)      \
    {                                   \
        bool update()                   \
        {                               \
            return registry().update(); \
        }                               \
    }                                   \
    namespace Category

#define PERFKIT_DECLARE_SUBCATEGORY(CategoryHierarchy) \
    PERFKIT_GCFG_CAT(CategoryHierarchy)                \
    {                                                  \
        bool update();                                 \
    }                                                  \
    namespace CategoryHierarchy

#define PERFKIT_DECLARE_CATEGORY(CategoryHierarchy) \
    PERFKIT_GCFG_CAT_ROOT(CategoryHierarchy)        \
    {                                               \
        bool update();                              \
    }                                               \
    namespace CategoryHierarchy

#define PERFKIT_CONFIGURE(name, ...) \
    PERFKIT_GCFG(name, __VA_ARGS__)

#define PERFKIT_T_CATEGORY(varname, ...) \
    PERFKIT_CFG_CLASS(varname)           \
    {                                    \
        __VA_ARGS__                      \
    }

#define PERFKIT_T_SUBCATEGORY(varname, ...)                            \
    PERFKIT_CFG_CLASS(INTERNAL_CPPH_CONCAT(_internal_subc_, varname)){ \
                                                                       \
            __VA_ARGS__};                                              \
    PERFKIT_CFG_SUBSET(INTERNAL_CPPH_CONCAT(_internal_subc_, varname), varname)

#define PERFKIT_T_CONFIGURE(ConfigName, ...) \
    PERFKIT_CFG(ConfigName, __VA_ARGS__)

#define PERFKIT_T_EXPAND_CATEGORY(varname, ...)          \
    struct varname : ::perfkit::config_class {           \
        INTERNAL_PERFKIT_T_CATEGORY_body(varname);       \
        explicit varname(::perfkit::config_class* other) \
                : _perfkit_INTERNAL_RG(other->_rg())     \
        {                                                \
        }                                                \
                                                         \
        __VA_ARGS__;                                     \
    };
