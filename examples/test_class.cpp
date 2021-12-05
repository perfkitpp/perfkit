//
// Created by ki608 on 2021-11-27.
//

#include "test_class.hpp"

#include <spdlog/spdlog.h>

PERFKIT_CATEGORY(test_global_category)
{
    PERFKIT_CONFIGURE(int_config, 1).confirm();
    PERFKIT_CONFIGURE(double_config, .1).confirm();
    PERFKIT_CONFIGURE(bool_config, false).confirm();
    PERFKIT_CONFIGURE(string_config, "dfle").confirm();

    PERFKIT_SUBCATEGORY(class_control)
    {
        PERFKIT_CONFIGURE(interval_ms, 50).confirm();
    }
}

using namespace std::literals;
namespace tc = test_global_category;

void test_class::start()
{
    _loop_active.store(true);
    _worker = std::thread{
            [&]() {
                CPPH_INFO("worker thread created.");

                while (_loop_active.load())
                {
                    auto trc_root = _tracer->fork("time_all");
                    auto& trc     = *_tracer;

                    trc.timer("global-update"), tc::update();
                    trc.timer("sleep-interval"),
                            trc.branch("duration-ms") = tc::class_control::interval_ms.value(),
                            std::this_thread::sleep_for(
                                    1ms * tc::class_control::interval_ms.value());

                    _cfg->update();

                    if (auto tr0 = trc.timer("tree-0"))
                    {
                        auto tr1       = trc.timer("tree-1");
                        tr1["integer"] = _cfg.t_int.ref();
                        tr1["double"]  = _cfg.t_double.value();
                        tr1["str"]     = _cfg.t_string.value();
                    }

                    auto tr1       = trc.timer("tree-1");
                    tr1["integer"] = _cfg.t_int.ref();
                    tr1["double"]  = _cfg.t_double.value();
                    tr1["str"]     = _cfg.t_string.value();

                    if (_cfg.t_boolean.ref())
                    {
                        auto tr2       = trc.timer("tree-2");
                        tr2["integer"] = _cfg.t_int.ref();
                        tr2["double"]  = _cfg.t_double.value();
                        tr2["str"]     = _cfg.t_string.value();
                    }
                }

                CPPH_INFO("disposing worker thread.");
            }};
}

void test_class::stop()
{
    _loop_active.store(false);
    _worker.joinable() && (_worker.join(), 1);
}
