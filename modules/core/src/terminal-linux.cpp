#include <cstring>
#include <string>

#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace perfkit::platform {
bool poll_stdin(int ms_to_wait, std::string* out_content)
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

    auto& bytes = *out_content;
    bytes.resize(n_read);

    auto n_read_real = ::read(STDIN_FILENO, bytes.data(), n_read);

    if (n_read_real != n_read) {
        return false;
    }

    if (not bytes.empty() && bytes.back() == '\n') {
        bytes.pop_back();
    }

    return true;
}
}  // namespace perfkit::platform