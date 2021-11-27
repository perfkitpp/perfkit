#include "utils.hpp"

#include <spdlog/spdlog.h>

#include "perfkit/common/macros.hxx"
#include "perfkit/detail/base.hpp"

#define CPPH_LOGGER() perfkit::glog()

#if __unix__
#    include <poll.h>
#    include <sys/ioctl.h>
#    include <unistd.h>

std::string perfkit::terminal::net::detail::try_fetch_input(int ms_to_wait)
{
    pollfd pollee;
    pollee.fd      = STDIN_FILENO;
    pollee.events  = POLLIN;
    pollee.revents = 0;

    if (::poll(&pollee, 1, ms_to_wait) <= 0)
        return {};  // nothing to read.

    int n_read = 0;
    ::ioctl(STDIN_FILENO, FIONREAD, &n_read);

    std::string bytes;
    bytes.resize(n_read);

    auto n_read_real = ::read(STDIN_FILENO, bytes.data(), n_read);

    if (n_read_real != n_read)
    {
        CPPH_ERROR("failed to read appropriate bytes from STDIN_FD!");
        return {};
    }

    return bytes;
}
#endif
