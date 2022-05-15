#include <csetjmp>
#include <csignal>

#include "cpph/timer.hxx"
#include "perfkit-appbase.hpp"
#include "perfkit/configs.h"
#include "perfkit/terminal.h"
#include "spdlog/spdlog.h"

// Turn off
static std::atomic_bool g_server_is_alive = true;
static std::weak_ptr<perfkit::if_terminal> g_weak_term;

// Handle sigint
static void sigint_handler(int signum)
{
    static int times = 5;
    spdlog::info("SIGINT received. Force exit: {} / {}", --times, 5);
    g_server_is_alive = false;

    if (times == 0) {
        spdlog::warn("Next SIGINT will terminate this process.");
        signal(SIGINT, nullptr);
    } else {
        if (auto term = g_weak_term.lock()) { term->push_command("quit"); }
        signal(SIGINT, &sigint_handler);
    }
}

std::unique_ptr<perfkit::AppBase> INTERNAL_perfkit_create_app();

int main(int argc, char** argv)
{
    using namespace cpph;

    spdlog::info("{}", argv[0]);
    spdlog::info("Creating application instance ...");
    auto app = INTERNAL_perfkit_create_app();

    try {
        spdlog::info("Parsing {} command line arguments", argc);
        perfkit::configs::parse_args(&argc, &argv, true);
    } catch (std::exception& e) {
        spdlog::error("Error during flag parsing: \n{}", e.what());
        return 1;
    }

    spdlog::info("APP INIT 0 - PreLoadConfigs");
    app->S01_PreLoadConfigs();

    spdlog::info("Loading configs ...");
    std::string confpath = argc == 1 ? app->DefaultConfigPath() : argv[1];
    {
        if (perfkit::configs::import_file(confpath)) {
            spdlog::info("Successfully loaded intial config '{}'", confpath);
        } else if (confpath.empty()) {
            spdlog::critical("Config path is not specified!");
            return 1;
        } else {
            spdlog::warn("Failed to load config '{}'", confpath);
        }
    }

    if (argc > 2) {
        spdlog::info("APP INIT 0 - Processing {} positional commandline args ...", argc - 2);
        app->S00_Opt_InitWithCmdLineArgs({(char const**)argv, size_t(argc - 2)});
    }

    spdlog::info("APP INIT 1 - PostLoadConfigs");
    app->S02_PostLoadConfigs();

    auto term = app->CreatePerfkitTerminal();
    g_weak_term = term;
    perfkit::terminal::register_conffile_io_commands(term.get());

    // Install signal handler
    signal(SIGINT, &sigint_handler);

    // Initialize application
    spdlog::info("Launching application ...");
    app->S03_LaunchApplication();

    // Configure commands
    spdlog::info("Adding application configs ...");
    term->add_command("quit", [] { g_server_is_alive = false; });
    app->S04_ConfigureTerminalCommands(term.get());

    // Queue save command
    term->push_command("save-config " + std::string(confpath));
    app->S05_SetInitialCommands(term.get());

    // Command receive loop
    stopwatch sw;

    while (g_server_is_alive) {
        app->Tick_Application(sw.elapsed().count()), sw.reset();
        term->invoke_queued_commands(
                app->DesiredTickInterval(),
                [] { return g_server_is_alive.load(); },
                [](auto cmd) { spdlog::info("CMD: {}", cmd); });
    }

    spdlog::info("Disposing application");
    app->P01_DisposeApplication();

    spdlog::info("Destroying application instance");
    app.reset();

    spdlog::info("Disposing terminal.");
    term.reset();

    return 0;
}
