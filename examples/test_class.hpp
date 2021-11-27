//
// Created by ki608 on 2021-11-27.
//

#pragma once
#include <perfkit/configs.h>
#include <perfkit/traces.h>

PERFKIT_T_CATEGORY(
        test_class_configs,

        PERFKIT_T_CONFIGURE(t_int, 10).confirm();
        PERFKIT_T_CONFIGURE(t_double, 10.).confirm();
        PERFKIT_T_CONFIGURE(t_string, "hell!").confirm();
        PERFKIT_T_CONFIGURE(t_boolean, true).confirm();

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
    std::string _id;
    test_class_configs _cfg{_id};
    perfkit::tracer_ptr _tracer = perfkit::tracer::create(1024, _id);
    std::thread _worker;

    std::atomic_bool _loop_active{true};
};
