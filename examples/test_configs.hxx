#pragma once
#include "perfkit/configs.h"
#include "perfkit/traces.h"

using namespace std::literals;

std::map<std::string, std::map<std::string, std::string>> ved{
        {    "asd",        {{"asd", "weqw"}, {"vafe, ewqew", "dwrew"}}},
        {"vadsfew", {{"dav ,ea w", "Ewqsad"}, {"scxz ss", "dwqewqew"}}}
};

PERFKIT_CATEGORY(cfg)
{
    PERFKIT_CONFIGURE(active, true).confirm();
    PERFKIT_CONFIGURE(active_async, true).confirm();
    PERFKIT_SUBCATEGORY(labels)
    {
        PERFKIT_CONFIGURE(foo, 1)
                .flags()
                .confirm();
        PERFKIT_CONFIGURE(bar, false)
                .flags()
                .confirm();
        PERFKIT_CONFIGURE(ce, "ola ollalala")
                .flags()
                .confirm();
        PERFKIT_CONFIGURE(cppr, "ola ollalala")
                .flags()
                .confirm();
        PERFKIT_CONFIGURE(ccp, "ola ollalala")
                .flags()
                .confirm();
        PERFKIT_CONFIGURE(ced, std::vector({1, 2, 3, 4, 5, 6})).confirm();
        PERFKIT_CONFIGURE(cedr, (std::map<std::string, int>{
                                        { "fdf", 2},
                                        {"erwe", 4}
        }))
                .confirm();
        PERFKIT_CONFIGURE(bb, (std::map<std::string, bool>{
                                      { "fdf", false},
                                      {"erwe",  true}
        }))
                .confirm();
        PERFKIT_CONFIGURE(cedrs, 3.141592).confirm();
        PERFKIT_CONFIGURE(cedrstt, std::move(ved)).confirm();
    }
    PERFKIT_SUBCATEGORY(lomo)
    {
        PERFKIT_SUBCATEGORY(movdo)
        {
            PERFKIT_CONFIGURE(ce, 1).confirm();
            PERFKIT_CONFIGURE(ced, 1).confirm();
            PERFKIT_CONFIGURE(cedr, 1).confirm();

            PERFKIT_SUBCATEGORY(cef)
            {
            }
            PERFKIT_SUBCATEGORY(ccra)
            {
                PERFKIT_CONFIGURE(foo, 1).confirm();
                PERFKIT_CONFIGURE(bar, 1).confirm();
                PERFKIT_CONFIGURE(ce, 1).confirm();
                PERFKIT_CONFIGURE(ced, 1).confirm();
                PERFKIT_CONFIGURE(cedr, 1).confirm();
                PERFKIT_CONFIGURE(cedrs, 1).confirm();

                PERFKIT_CONFIGURE(a_foo, 1).confirm();
                PERFKIT_CONFIGURE(a_bar, 1).confirm();
                PERFKIT_CONFIGURE(a_ce, 1).confirm();
                PERFKIT_CONFIGURE(a_ced, 1).confirm();
                PERFKIT_CONFIGURE(a_cedr, 1).confirm();
                PERFKIT_CONFIGURE(a_cedrs, 1).confirm();

                PERFKIT_CONFIGURE(b_foo, 1).confirm();
                PERFKIT_CONFIGURE(b_bar, 1).confirm();
                PERFKIT_CONFIGURE(b_ce, 1).confirm();
                PERFKIT_CONFIGURE(b_ced, 1).confirm();
                PERFKIT_CONFIGURE(b_cedr, 1).confirm();
                PERFKIT_CONFIGURE(b_cedrs, 1).confirm();

                PERFKIT_CONFIGURE(c_foo, 1).confirm();
                PERFKIT_CONFIGURE(c_bar, 1).confirm();
                PERFKIT_CONFIGURE(c_ce, 1).confirm();
                PERFKIT_CONFIGURE(c_ced, 1).confirm();
                PERFKIT_CONFIGURE(c_cedr, 1).confirm();
                PERFKIT_CONFIGURE(c_cedrs, 1).confirm();

                PERFKIT_CONFIGURE(d_foo, 1).confirm();
                PERFKIT_CONFIGURE(d_bar, 1).confirm();
                PERFKIT_CONFIGURE(d_ce, 1).confirm();
                PERFKIT_CONFIGURE(d_ced, 1).confirm();
                PERFKIT_CONFIGURE(d_cedr, 1).confirm();
                PERFKIT_CONFIGURE(d_cedrs, 1).confirm();
            }
        }
    }

    PERFKIT_CONFIGURE(foo, 1).confirm();
    PERFKIT_CONFIGURE(bar, 1).confirm();
    PERFKIT_CONFIGURE(ce, 1).confirm();
    PERFKIT_CONFIGURE(ced, 1).confirm();
    PERFKIT_CONFIGURE(cedr, 1).confirm();
    PERFKIT_CONFIGURE(cedrs, 1).confirm();

    PERFKIT_CONFIGURE(a_foo, 1).confirm();
    PERFKIT_CONFIGURE(a_bar, 1).confirm();
    PERFKIT_CONFIGURE(a_ce, 1).confirm();
    PERFKIT_CONFIGURE(a_ced, 1).confirm();
    PERFKIT_CONFIGURE(a_cedr, 1).confirm();
    PERFKIT_CONFIGURE(a_cedrs, 1).confirm();

    PERFKIT_CONFIGURE(b_foo, 1).confirm();
    PERFKIT_CONFIGURE(b_bar, 1).confirm();
    PERFKIT_CONFIGURE(b_ce, 1).confirm();
    PERFKIT_CONFIGURE(b_ced, 1).confirm();
    PERFKIT_CONFIGURE(b_cedr, 1).confirm();
    PERFKIT_CONFIGURE(b_cedrs, 1).confirm();

    PERFKIT_CONFIGURE(c_foo, 1).confirm();
    PERFKIT_CONFIGURE(c_bar, 1).confirm();
    PERFKIT_CONFIGURE(c_ce, 1).confirm();
    PERFKIT_CONFIGURE(c_ced, 1).confirm();
    PERFKIT_CONFIGURE(c_cedr, 1).confirm();
    PERFKIT_CONFIGURE(c_cedrs, 1).confirm();

    PERFKIT_CONFIGURE(d_foo, 1).confirm();
    PERFKIT_CONFIGURE(d_bar, 1).confirm();
    PERFKIT_CONFIGURE(d_ce, 1).confirm();
    PERFKIT_CONFIGURE(d_ced, 1).confirm();
    PERFKIT_CONFIGURE(d_cedr, 1).confirm();
    PERFKIT_CONFIGURE(d_cedrs, 1).confirm();

    PERFKIT_CONFIGURE(e_foo, 1).confirm();
    PERFKIT_CONFIGURE(e_bar, 1).confirm();
    PERFKIT_CONFIGURE(e_ce, 1).confirm();
    PERFKIT_CONFIGURE(e_ced, 1).confirm();
    PERFKIT_CONFIGURE(e_cedr, 1).confirm();
    PERFKIT_CONFIGURE(e_cedrs, 1).confirm();
}

PERFKIT_CATEGORY(vlao) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao1) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao2) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao3) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao4) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao5) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao6) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao7) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao8) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao9) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao22) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao33) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao44) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao55) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }

auto trace_a = perfkit::tracer::create(0, ".. Trace A");
auto trace_b = perfkit::tracer::create(1, ".. Trace B");

void do_trace(size_t ic, std::string cmd)
{
    auto trc_root        = trace_a->fork("a,  b ,   c ,   d", 0);
    auto trc_b           = trace_b->fork("a,  b ,   c ,   d, e e    f,    g", 0);
    trc_b["placeholder"] = "vlvlvl";
    trc_b["command"]     = "cmd";

    auto timer                         = trc_root.timer("Some Timer");
    trc_root["Value 0"]                = 3;
    trc_root["Value 1"]                = *cfg::labels::foo;
    trc_root["Value 2"]                = fmt::format("Hell, world! {}", *cfg::labels::foo);
    trc_root["Value 3"]                = false;
    trc_root["Value 3"]["Subvalue 0"]  = ic;
    trc_root["Value 3"]["Subvalue GR"] = std::vector<int>{3, 4, 5};
    trc_root["Value 3"]["Subvalue 1"]  = double(ic);
    trc_root["Value 3"]["Subvalue 2"]  = !!(ic & 1);
    trc_root["Value 4"]["Subvalue 3"]  = fmt::format("Hell, world! {}", ic);

    auto r                            = trc_root["Value 5"];
    trc_root["Value 5"]["Subvalue 0"] = ic;
    if (r) { trc_root["Value 5"]["Subvalue 1 Cond"] = double(ic); }
    trc_root["Value 5"]["Subvalue 2"] = !!(ic & 1);
}
