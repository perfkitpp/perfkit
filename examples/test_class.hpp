//
// Created by ki608 on 2021-11-27.
//

#pragma once
#include <perfkit/common/helper/nlohmann_json_macros.hxx>
#include <perfkit/configs.h>
#include <perfkit/logging.h>
#include <perfkit/traces.h>

struct test_config_structure
{
    int x            = 0;
    int y            = 0;
    double z         = 0;
    bool flag        = false;
    std::string path = "ola";

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            test_config_structure,
            x, y, z, flag, path);
};

PERFKIT_T_CATEGORY(
        test_class_configs,

        PERFKIT_T_CONFIGURE(t_int, 10)
                .min(0)
                .max(141)
                .confirm();

        PERFKIT_T_CONFIGURE(t_double, 10.)
                .min(4)
                .max(181)
                .confirm();

        PERFKIT_T_CONFIGURE(t_string, "hell!").confirm();

        PERFKIT_T_CONFIGURE(t_boolean, true).confirm();

        PERFKIT_T_CONFIGURE(t_intarray, std::array<int, 3>{})
                .min({4, 3, 1})
                .max({61, 88, 71})
                .confirm();

        PERFKIT_T_CONFIGURE(t_intvector, std::vector<int>{{3, 45, 1, 41}}).confirm();

        PERFKIT_T_SUBCATEGORY(
                subconfigs,

                PERFKIT_T_CONFIGURE(custom_struct, test_config_structure{}).confirm();

        );

);

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
    test_class_configs _cfg{_id};
    perfkit::tracer_ptr _tracer = perfkit::tracer::create(1024, _id);
    std::thread _worker;

    std::atomic_bool _loop_active{true};
    perfkit::logger_ptr _logger = perfkit::share_logger(_id);
};
