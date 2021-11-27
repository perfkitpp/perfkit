#pragma once
#include <functional>
#include <string>

#include "perfkit/common/utility/inserter.hxx"

namespace perfkit::terminal::net::detail
{
std::string try_fetch_input(int ms_to_wait);

void input_redirect(std::function<void(char)> inserter);
void input_rollback();
}  // namespace perfkit::terminal::net::detail
