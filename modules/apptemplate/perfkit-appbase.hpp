#pragma once
#include <string>

#include <perfkit/terminal.h>

int main(int argc, char** argv);

namespace perfkit {
class AppBase
{
    friend int ::main(int, char**);

   public:
    virtual ~AppBase() noexcept = default;

   public:
    virtual std::string DefaultConfigPath() const noexcept { return ""; }
    virtual int S00_ParseCommandLineArgs(int argc, char** argv);
    virtual void S01_PreLoadConfigs() {}

    virtual void S02_PostLoadConfigs() {}
    virtual void S03_LaunchApplication() {}
    virtual void S04_ConfigureTerminalCommands(if_terminal*) {}
    virtual void S05_SetInitialCommands(if_terminal*) {}
    virtual void Tick_Application(float delta_time) {}
    virtual void P01_DisposeApplication() {}

    virtual bool HasCustomConfig() const { return false; }
    virtual void CustomLoadConfig() {}
    virtual void CustomSaveConfig() {}

   public:
    //! Invokes right after S02_PostLoadConfigs
    virtual auto CreatePerfkitTerminal() -> perfkit::terminal_ptr = 0;
    virtual auto DesiredTickInterval() const -> milliseconds { return milliseconds{200}; }
    virtual auto LogServiceTickInterval() const -> milliseconds { return milliseconds{300}; }
};
}  // namespace perfkit

#define PERFKIT_USE_APP_CLASS(YourAppClass)                         \
    std::unique_ptr<perfkit::AppBase> INTERNAL_perfkit_create_app() \
    {                                                               \
        return std::make_unique<YourAppClass>();                    \
    }
