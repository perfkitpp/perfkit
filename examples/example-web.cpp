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

#include <cpph/utility/singleton.hxx>

#include "perfkit-appbase.hpp"
#include "perfkit/web.h"
#include "spdlog/common.h"
#include "spdlog/spdlog.h"
#include "test_class.hpp"

class ExampleWebApp : public perfkit::AppBase
{
    test_class *test1_ = {}, *test2_ = {}, *test3_ = {};

   public:
    int S00_ParseCommandLineArgs(int argc, char** argv) override
    {
        spdlog::set_level(spdlog::level::trace);
        return AppBase::S00_ParseCommandLineArgs(argc, argv);
    }

    void S04_ConfigureTerminalCommands(perfkit::if_terminal* terminal) override
    {
        AppBase::S04_ConfigureTerminalCommands(terminal);

        test1_ = new test_class{"test1"};
        test2_ = new test_class{"test2"};
        test3_ = new test_class{"test3"};

        test1_->start();
        test2_->start();
        test3_->start();
    }

    void P01_DisposeApplication() override
    {
        AppBase::P01_DisposeApplication();

        delete test1_;
        delete test2_;
        delete test3_;
    }

   public:
    auto CreatePerfkitTerminal() -> perfkit::terminal_ptr override
    {
        return perfkit::web::open();
    }

    std::string DefaultConfigPath() const noexcept override
    {
        return "../../.example-web.json";
    }
};

PERFKIT_USE_APP_CLASS(ExampleWebApp);
