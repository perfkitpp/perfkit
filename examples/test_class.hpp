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

//
// Created by ki608 on 2021-11-27.
//

#pragma once
#include <list>

#include <cpph/refl/types/nlohmann_json_fwd.hpp>
#include <perfkit/configs.h>
#include <perfkit/logging.h>
#include <perfkit/traces.h>

struct test_config_structure_nested {
    int x = 0;
    int y = 0;
    double z = 0;
    bool flag = false;
    std::string path = "ola";
    std::list<test_config_structure_nested> list;

    CPPH_REFL_DEFINE_OBJECT_inline_simple(x, y, z, flag, path, list);
};

struct test_config_structure {
    int x = 0;
    int y = 0;
    double z = 0;
    bool flag = false;
    std::string path = "ola";

    test_config_structure_nested nested = {test_config_structure_nested{}};

    CPPH_REFL_DEFINE_OBJECT_inline_simple(x, y, z, flag, path, nested);
};

PERFKIT_CFG_CLASS(test_subclass)
{
    PERFKIT_CFG(my_var_1, 0);
    PERFKIT_CFG(my_var_2, 0);
    PERFKIT_CFG(my_var_3, 0);
    PERFKIT_CFG(my_var_4, 0);

    PERFKIT_CFG_CLASS(test_subclass_2)
    {
        PERFKIT_CFG(subvar_1, 1.);
        PERFKIT_CFG(subvar_2, 2.);
        PERFKIT_CFG(subvar_3, 3.);
        PERFKIT_CFG(subvar_4, 4.);
    };

    PERFKIT_CFG_SUBSET(test_subclass_2, my_subclass_1);
};

PERFKIT_T_CATEGORY(
        test_class_configs,

        PERFKIT_T_CONFIGURE(t_increment, 10)
                .confirm();

        PERFKIT_T_CONFIGURE(t_increment_inplace, 10)
                .confirm();

        PERFKIT_T_CONFIGURE(t_int, 10)
                .min(0)
                .max(141)
                .confirm();

        PERFKIT_T_CONFIGURE(t_int_with_min, 10)
                .min(1)
                .confirm();

        PERFKIT_T_CONFIGURE(t_int_with_max, 10)
                .max(1516)
                .confirm();

        PERFKIT_T_CONFIGURE(t_double, 10.)
                .min(4)
                .max(181)
                .confirm();

        PERFKIT_T_CONFIGURE(t_string, "hell!").confirm();

        PERFKIT_T_CONFIGURE(t_boolean, false).confirm();

        //        PERFKIT_T_CONFIGURE(t_json, nlohmann::json{}).confirm();

        PERFKIT_T_CONFIGURE(t_intarray, std::array<int, 3>{})
                .min({4, 3, 1})
                .max({61, 88, 71})
                .confirm();

        PERFKIT_T_CONFIGURE(t_string_oneof, "")
                .one_of({"hello", "string2", "string3", "string4", "ohoho"})
                .confirm();

        PERFKIT_T_CONFIGURE(t_intvector, std::vector<int>{{3, 45, 1, 41}}).confirm();

        PERFKIT_T_SUBCATEGORY(
                subconfigs,

                PERFKIT_T_CONFIGURE(custom_struct, test_config_structure{})
                        .min({0, 0, 0., false, "ola olala!"})
                        .max({251, 311, 0., false, "vl vl!!"})
                        .confirm();

                PERFKIT_CFG(average, 3);
                PERFKIT_CFG(mydark, 3);
                PERFKIT_CFG(vofafa, 3);
                PERFKIT_CFG(myhoers, 3);

                PERFKIT_T_SUBCATEGORY(
                        subsubconfigs,

                        PERFKIT_CFG(fofofo, 1);
                        PERFKIT_CFG(aldf, 3);););

        PERFKIT_T_CONFIGURE(custom_struct, test_config_structure{})
                .min({0, 0, 0., false, "ola olala!"})
                .max({251, 311, 0., false, "vl vl!!"})
                .confirm(););

class test_class
{
   public:
    explicit test_class(std::string class_id)
            : _id(std::move(class_id))
    {
    }

    ~test_class()
    {
        stop();
    }

   public:
    void stop();
    void start();

   private:
    auto CPPH_LOGGER() const { return &*_logger; }

   private:
    std::string _id;
    test_class_configs _cfg = test_class_configs::create(_id);
    perfkit::tracer_ptr _tracer = perfkit::tracer::create(1024, _id);
    std::thread _worker;

    std::atomic_bool _loop_active{true};
    perfkit::logger_ptr _logger = perfkit::share_logger(_id);

    std::list<test_subclass> _subclasses;
};
