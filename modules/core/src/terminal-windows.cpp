#include <chrono>
#include <string>
#include <thread>

#include <conio.h>

using namespace std::literals;

namespace perfkit::platform {
bool poll_stdin(int ms_to_wait, std::string* out_content)
{
    auto& buffer = *out_content;

    auto  now = [] { return std::chrono::steady_clock::now(); };
    auto  until = 1ms * ms_to_wait + now();

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
                return true;
            } else {
                continue;
            }
        }

        std::this_thread::sleep_for(10ms);
    }

    return false;
}
}  // namespace perfkit::platform