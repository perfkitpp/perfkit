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

#include "utils.hpp"

#include <charconv>
#include <csignal>
#include <thread>

#include <spdlog/spdlog.h>

#include "cpph/assert.hxx"
#include "cpph/counter.hxx"
#include "cpph/futils.hxx"
#include "cpph/macros.hxx"
#include "cpph/timer.hxx"
#include "cpph/utility/ownership.hxx"
#include "cpph/zip.hxx"
#include "perfkit/detail/base.hpp"
#include "perfkit/detail/logging.hpp"

#define CPPH_LOGGER() perfkit::net::detail::nglog()

spdlog::logger* perfkit::net::detail::nglog()
{
    static auto logger = perfkit::share_logger("PERFKIT:NET");
    return &*logger;
}

#if __unix__
#    include <poll.h>
#    include <sys/ioctl.h>
#    include <unistd.h>

std::string perfkit::net::detail::try_fetch_input(int ms_to_wait)
{
    pollfd pollee;
    pollee.fd = STDIN_FILENO;
    pollee.events = POLLIN;
    pollee.revents = 0;

    if (::poll(&pollee, 1, ms_to_wait) <= 0) {
        return {};
    }

    int n_read = 0;
    ::ioctl(STDIN_FILENO, FIONREAD, &n_read);

    std::string bytes;
    bytes.resize(n_read);

    auto n_read_real = ::read(STDIN_FILENO, bytes.data(), n_read);

    if (n_read_real != n_read) {
        CPPH_ERROR("failed to read appropriate bytes from STDIN_FD!");
        return {};
    }

    if (not bytes.empty() && bytes.back() == '\n') {
        bytes.pop_back();
    }

    return bytes;
}

static struct redirection_context_t {
   private:
    int              _fd_org_stdout = -1;
    int              _fd_org_stderr = -1;
    int              _fd_pout[2] = {-1, -1};
    int              _fd_perr[2] = {-1, -1};

    std::atomic_bool _active = false;
    std::thread      _worker;

   public:
    void rollback()
    {
        if (_fd_org_stderr == -1)
            return;

        _active = false;
        _worker.joinable() && (_worker.join(), 1);

        dup2(_fd_org_stdout, STDOUT_FILENO);
        dup2(_fd_org_stderr, STDERR_FILENO);

        for (auto fd : {
                     &_fd_org_stdout,
                     &_fd_org_stderr,
                     &_fd_pout[0],
                     &_fd_pout[1],
                     &_fd_perr[0],
                     &_fd_perr[1],
             }) {
            if (*fd == -1)
                continue;

            close(*fd);
            *fd = -1;
        }
    }

    void redirect(std::function<void(char const*, size_t)> fn)
    {
        _fd_org_stdout = dup(STDOUT_FILENO);
        _fd_org_stderr = dup(STDERR_FILENO);

        if (pipe(_fd_pout) == -1)
            goto ANY_ERROR;

        if (pipe(_fd_perr) == -1)
            goto ANY_ERROR;

        dup2(_fd_pout[1], STDOUT_FILENO);
        dup2(_fd_perr[1], STDERR_FILENO);

        CPPH_INFO("stdout, stderr redirected to pipe {}, {}", _fd_pout[1], _fd_perr[1]);

        _active = true;
        _worker = std::thread(
                [this, fn = std::move(fn)] {
                    pollfd pollfds[2];
                    pollfds[0].fd = _fd_pout[0];
                    pollfds[1].fd = _fd_perr[0];
                    pollfds[0].events = POLLIN;
                    pollfds[1].events = POLLIN;

                    char buffer[2048];

                    while (_active) {
                        for (auto& pfd : pollfds)
                            pfd.revents = 0;

                        auto n_sig = ::poll(pollfds, std::size(pollfds), 500);

                        if (n_sig == 0) {
                            continue;
                        } else if (n_sig < 0) {
                            CPPH_ERROR("::poll() error! ({}) {}", errno, strerror(errno));
                            continue;
                        }

                        int src_fd[] = {_fd_org_stdout, _fd_org_stderr};
                        for (auto [pfd, fd] : perfkit::zip(pollfds, src_fd)) {
                            if (not(pfd->revents & POLLIN))
                                continue;

                            auto n_read = ::read(pfd->fd, buffer, std::size(buffer));
                            auto n_write = ::write(*fd, buffer, n_read);

                            fn(buffer, n_read);
                        }
                    }
                });

        return;

    ANY_ERROR:
        CPPH_ERROR("failed to redirect stdstream");
        rollback();
    }

    ~redirection_context_t()
    {
        rollback();
    }
} g_redirect;

void perfkit::net::detail::input_redirect(std::function<void(char const*, size_t)> inserter)
{
    g_redirect.rollback();
    g_redirect.redirect(std::move(inserter));
}

void perfkit::net::detail::input_rollback()
{
    g_redirect.rollback();
}

bool perfkit::net::detail::fetch_proc_stat(perfkit::net::detail::proc_stat_t* ostat)
{
    try {
        perfkit::stopwatch trace;

        static double      uptime = 0.;
        static double      delta_time = 0.;
        {
            futils::file_ptr ptr{fopen("/proc/uptime", "r")};

            double           now;
            fscanf(&*ptr, "%lf", &now);

            delta_time = now - uptime;
            uptime = now;
        }

        struct _all {
            int64_t user = 0;
            int64_t system = 0;
            int64_t nice = 0;
            int64_t idle = 0;
            int64_t wait = 0;
            int64_t hi = 0;
            int64_t si = 0;

            void    fill(proc_stat_t* out) const noexcept
            {
                static int64_t prev_total, prev_user, prev_nice, prev_idle;

                auto           total = user + nice + system + idle + wait + hi + si;
                auto           total_delta = total - prev_total;
                auto           user_delta = user + nice - prev_user - prev_nice;
                auto           idle_delta = idle - prev_idle;

                auto           divider = total_delta / delta_time;
                auto           userm = double(user_delta) / divider;
                auto           systemm = double(total_delta - user_delta - idle_delta) / divider;

                prev_user = user;
                prev_nice = nice;
                prev_idle = idle;
                prev_total = total;

                out->cpu_usage_total_user = userm;
                out->cpu_usage_total_system = systemm;
            }
        };

        {  // retrieve system
            auto [buffer, size] = futils::readin("/proc/stat");
            auto content = std::string_view(buffer.get(), size);
            char cpubuf[4];

            _all all;
            sscanf(buffer.get(),
                   "%s %ld %ld %ld %ld %ld %ld %ld 0",
                   cpubuf,
                   &all.user, &all.nice, &all.system, &all.idle,
                   &all.wait, &all.hi, &all.si);

            all.fill(ostat);
        }

        {  // retrieve self

            auto [buffer, size] = futils::readin("/proc/self/stat");
            auto content = std::string_view(buffer.get(), size);

            content = content.substr(content.find_last_of(')') + 2);
            int  cursor = 3;

            auto get_at
                    = ([&](int destination) -> int64_t {
                          assert(destination >= cursor);

                          for (; cursor < destination; ++cursor)
                              content = content.substr(content.find_first_of(' ') + 1);

                          auto    begin = content.data();
                          auto    end = begin + content.find_first_of(' ');
                          int64_t retval;

                          std::from_chars(begin, end, retval);
                          return retval;
                      });

            static const auto sysclock{sysconf(_SC_CLK_TCK)};
            static int64_t    prev_utime, prev_stime;

            auto              sysclockf = double(sysclock);
            auto              utime = get_at(14);
            auto              stime = get_at(15);
            auto              num_thrd = get_at(20);
            auto              vsize = get_at(23);
            auto              rss = get_at(24) * sysconf(_SC_PAGESIZE);

            ostat->cpu_usage_self_user = ((utime - prev_utime) / delta_time / sysclockf);
            ostat->cpu_usage_self_system = ((stime - prev_stime) / delta_time / sysclockf);
            ostat->num_threads = num_thrd;
            ostat->memory_usage_virtual = vsize;
            ostat->memory_usage_resident = rss;

            prev_utime = utime;
            prev_stime = stime;
        }
    } catch (futils::file_read_error& e) {
        CPPH_ERROR("failed to read /proc/");
        return false;
    }

    return true;
}

void perfkit::net::detail::write(const char* buffer, size_t n)
{
    fwrite(buffer, 1, n, stdout);
}

#elif _WIN32
#    include <iostream>

#    define NOMINMAX
#    include <Windows.h>
//
#    include <Psapi.h>
#    include <TlHelp32.h>
#    include <conio.h>
#    include <spdlog/sinks/base_sink.h>

#    include "cpph/algorithm/std.hxx"

using namespace std::literals;

std::string perfkit::net::detail::try_fetch_input(int ms_to_wait)
{
    static std::string buffer;

    std::string        return_string;
    auto               now = [] { return std::chrono::steady_clock::now(); };
    auto               until = 1ms * ms_to_wait + now();

    while (now() < until) {
        if (_kbhit()) {
            auto ch = _getch();

            if (ch == '\r') {
                ch = '\n';
            } else {
                buffer += (char)ch;
            }

            putc(ch, stdout);
            if (ch == '\n') {
                return_string = buffer;
                buffer.clear();

                break;
            }
        }

        std::this_thread::sleep_for(10ms);
    }

    return return_string;
}

static unsigned long long FileTimeToInt64(const FILETIME& ft) { return (((unsigned long long)(ft.dwHighDateTime)) << 32) | ((unsigned long long)ft.dwLowDateTime); }

bool                      perfkit::net::detail::fetch_proc_stat(perfkit::net::detail::proc_stat_t* ostat)
{
    {  // Retrieve memory usage
        PROCESS_MEMORY_COUNTERS pmc;
        auto                    result = GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc);

        if (not result) {
            CPPH_WARN("GetProcessMemoryInfo() FAILED");
            return false;
        }

        ostat->memory_usage_virtual = pmc.PagefileUsage + pmc.QuotaNonPagedPoolUsage + pmc.QuotaPagedPoolUsage;
        ostat->memory_usage_resident = pmc.WorkingSetSize;
    }

    double totalDeltaTicks;

    {  // Retrieve global CPU usage
        static int64_t idleTimePrev, kernelTimePrev, userTimePrev;
        FILETIME       idleTime, kernelTime, userTime;
        if (not GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            CPPH_WARN("GetSystemTimes() FAILED");
            return false;
        }

        auto deltaIdle = FileTimeToInt64(idleTime) - idleTimePrev;
        auto deltaKernel = FileTimeToInt64(kernelTime) - kernelTimePrev;
        auto deltaUser = FileTimeToInt64(userTime) - userTimePrev;

        totalDeltaTicks = deltaIdle + deltaKernel + deltaUser;
        ostat->cpu_usage_total_system = deltaKernel / totalDeltaTicks;
        ostat->cpu_usage_total_user = deltaUser / totalDeltaTicks;

        idleTimePrev = FileTimeToInt64(idleTime);
        kernelTimePrev = FileTimeToInt64(kernelTime);
        userTimePrev = FileTimeToInt64(userTime);
    }

    {
        static const auto num_cores = std::thread::hardware_concurrency();
        static int64_t    kernelTimePrev, userTimePrev;
        FILETIME          kernelTime, userTime, _unused;
        if (not GetProcessTimes(GetCurrentProcess(), &_unused, &_unused, &kernelTime, &userTime)) {
            CPPH_WARN("GetProcessTimes() FAILED");
            return false;
        }

        auto deltaKernel = FileTimeToInt64(kernelTime) - kernelTimePrev;
        auto deltaUser = FileTimeToInt64(userTime) - userTimePrev;

        ostat->cpu_usage_self_system = deltaKernel / totalDeltaTicks * num_cores;
        ostat->cpu_usage_self_user = deltaUser / totalDeltaTicks * num_cores;

        kernelTimePrev = FileTimeToInt64(kernelTime);
        userTimePrev = FileTimeToInt64(userTime);
    }

    static perfkit::poll_timer thread_count_timer{5s};
    static size_t              num_threads;

    if (thread_count_timer.check()) {
        size_t nThread = 0;
        HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        auto   PID = GetProcessId(GetCurrentProcess());
        if (h != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te;
            te.dwSize = sizeof(te);
            if (Thread32First(h, &te)) {
                do {
                    if (te.th32OwnerProcessID == PID) {
                        ++nThread;
                    }
                    te.dwSize = sizeof(te);
                } while (Thread32Next(h, &te));
            }
            CloseHandle(h);
        }

        num_threads = nThread;
    }

    ostat->num_threads = num_threads;

    return true;
}

//! \see https://stackoverflow.com/questions/54094127/redirecting-stdout-in-win32-does-not-redirect-stdout
//! \see https://www.asawicki.info/news_1326_redirecting_standard_io_to_windows_console
class redirect_context_t : public spdlog::sinks::base_sink<std::mutex>
{
   public:
    redirect_context_t(std::function<void(char const*, size_t)> inserter)
            : _inserter(std::move(inserter))
    {
    }

   protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);

        _inserter(formatted.data(), formatted.size());
    }

    void flush_() override
    {
        ;
    }

   public:
    void operator()(char const* buf, size_t n)
    {
        _inserter(buf, n);
    }

   private:
    std::function<void(char const*, size_t)> _inserter;
};

static std::shared_ptr<redirect_context_t> inserter_sink;

void                                       perfkit::net::detail::input_redirect(std::function<void(char const*, size_t)> inserter)
{
    assert_(not inserter_sink);

    CPPH_INFO("Redirecting all registered loggers");

    inserter_sink = std::make_shared<redirect_context_t>(std::move(inserter));
    auto   default_logger_sink = spdlog::default_logger()->sinks().front();

    size_t n_redirected = 0;
    spdlog::details::registry::instance()
            .apply_all([&](std::shared_ptr<spdlog::logger> logger) {
                if (logger->sinks().size() == 1 && logger->sinks().front() == default_logger_sink) {
                    logger->sinks().push_back(inserter_sink);
                    ++n_redirected;
                }
            });

    CPPH_INFO("{} loggers redirected.", n_redirected);
}

void perfkit::net::detail::input_rollback()
{
    spdlog::details::registry::instance()
            .apply_all([&](std::shared_ptr<spdlog::logger> logger) {
                auto& sinks = logger->sinks();
                auto  it = perfkit::find(sinks, inserter_sink);

                if (it != sinks.end())
                    sinks.erase(it);
            });

    if (inserter_sink.use_count() != 1) {
        CPPH_WARN(
                "inserter_sink is cached elsewhere, {} references left.",
                inserter_sink.use_count());
    } else {
        CPPH_INFO("input rollbacked.");
    }

    inserter_sink.reset();
}

void perfkit::net::detail::write(const char* buffer, size_t n)
{
    fwrite(buffer, 1, n, stdout);

    if (inserter_sink)
        (*inserter_sink)(buffer, n);
}

#endif
