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

#include "test_class.hpp"

#include <spdlog/spdlog.h>

#include "cpph/utility/timer.hxx"

PERFKIT_CATEGORY(test_global_category)
{
    PERFKIT_CONFIGURE(int_config, 1).confirm();
    PERFKIT_CONFIGURE(double_config_with_long_text_name_example, .1).confirm();
    PERFKIT_CONFIGURE(bool_config, false)
            .description(
                    "장문의 한글 설명: 역사를 그들에게 보이는 쓸쓸하랴? 크고 석가는 얼음이 인생에 수 소리다."
                    "이것은 할지니, 실로 것이다. 목숨이 고행을 때까지 것이다. 이상 못하다 대중을 인생에 "
                    "그들은 피가 따뜻한 되려니와, 이것이다. 든 부패를 영락과 이 우는 수 거선의 안고, 운다. "
                    "있는 눈에 위하여 끓는다. 안고, 노년에게서 수 있는 운다. 군영과 타오르고 이 철환하였는가?"
                    " 우리는 밥을 스며들어 이는 속에 사막이다. 이것은 이상, 천자만홍이 자신과 이 불러 가지에 이상은 때문이다."
                    "투명하되 할지니, 사라지지 그들의 하여도 이 그들에게 인생에 쓸쓸하랴? 미묘한 남는 간에 "
                    "뛰노는 힘있다. 심장은 끝까지 하는 수 위하여 것이다.")
            .confirm();

    PERFKIT_CONFIGURE(kr_str, "한글 문자열").confirm();

    PERFKIT_CONFIGURE(string_config, "dfle")
            .description("한글 설명")
            .confirm();

    PERFKIT_SUBCATEGORY(class_control)
    {
        PERFKIT_CONFIGURE(interval_ms, 50).confirm();
    }
}

using namespace std::literals;
namespace tc = test_global_category;

void test_class::start()
{
    using fmt::format;
    _loop_active.store(true);
    _worker = std::thread{
            [this]() {
                CPPH_INFO("worker thread created.");
                bool reload_tracer_next_frame = false;
                cpph::poll_timer sleep;

                while (_loop_active.load()) {
                    if (_cfg.t_boolean) {
                        CPPH_INFO("LOOP!");
                    }

                    if (std::exchange(reload_tracer_next_frame, false)) {
                        _tracer->unregister();
                        _tracer.reset(), _tracer = perfkit::tracer::create(1024, _id);
                        CPPH_INFO("Reloading tracer 1");

                        _tracer->unregister();
                        _tracer.reset(), _tracer = perfkit::tracer::create(1024, _id);
                        CPPH_INFO("Reloading tracer 2 ");
                    }

                    thread_local static int sequencer = 0;
                    auto trc_root = _tracer->fork(fmt::format("tracer # {}", ++sequencer % 5));
                    auto& trc = *_tracer;

                    trc.timer("global-update"), tc::update();
                    trc.timer("sleep-interval"),
                            trc.branch("duration-ms") = tc::class_control::interval_ms.value(),
                            (sleep.check(), std::this_thread::sleep_until(sleep.next_point()));

                    _cfg->update();
                    _cfg.t_increment.commit(*_cfg.t_increment + 1);
                    _cfg.t_increment_inplace.commit_inplace(*_cfg.t_increment_inplace + 2);

                    if (auto to = 1ms * tc::class_control::interval_ms; to != sleep.interval())
                        sleep.reset(to);

                    if (auto tr0 = trc.timer("tree-0")) {
                        auto tr1 = trc.timer("tree-1");
                        auto t = tr1["integer"];

                        static int my_var = 0;
                        t = t ? ++my_var : my_var;

                        tr1["double"] = _cfg.t_double.value();
                        tr1["str"] = _cfg.t_string.value();
                        tr1["diffname"] = _cfg.t_string.value();

                        spdlog::info("한글 로그");
                    }

                    auto tr1 = trc.timer("tree-1");
                    tr1["integer"] = _cfg.t_int.ref();
                    tr1["double"] = _cfg.t_double.value();
                    tr1["str"] = _cfg.t_string.value();

                    if (trc.timer("create instant registry").check_subs()) {
                        static int counter = 0;
                        test_subclass::create(format("temporary+{}", ++counter));
                    }

                    if (trc.timer("add new subclass and dispose instantly").check_subs()) {
                        static int counter = 0;
                        test_subclass::create(_cfg, format("tempory instant disposed {}", ++counter));
                    }

                    if (trc.timer("add consistent subclass").check_subs()) {
                        static int counter = 0;
                        auto subc = test_subclass::create(_cfg, format("instant subclass {}", ++counter));
                        _subclasses.push_back(std::move(subc));
                    }

                    if (trc.timer("clear subclasses").check_subs()) {
                        _subclasses.clear();
                    }

                    if (_cfg.t_boolean.ref()) {
                        auto tr2 = trc.timer("tree-2");
                        tr2["integer"] = _cfg.t_int.ref();
                        tr2["double"] = _cfg.t_double.value();
                        tr2["str"] = _cfg.t_string.value();
                    }

                    if (auto branch = trc.branch("reload")) {
                        reload_tracer_next_frame = true;
                        branch.unsubscribe();
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
