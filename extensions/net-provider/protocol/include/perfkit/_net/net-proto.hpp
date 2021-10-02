#include <string_view>

#include <nlohmann/json.hpp>

namespace perfkit::_net {
constexpr std::string_view HEADER = "@Pe%Fk1t";

enum class session_message : uint8_t {
  invalid          = 0,
  register_session = 1,
  config_all       = 2,
  config_update    = 3,
  trace_all        = 4,
  trace_image      = 5,
};

enum class server_message : uint8_t {
  invalid      = 0,
  heartbeat    = 1,
  config_fetch = 2, // fetches accumulated updates
};

#pragma pack(push, 1)

// -- Manipulating Configs --
// config_all: 모든 컨피겨레이션 서술자를 내보냅니다.
//    1. 키 값에 기반한 해시
//    2. 모든 어트리뷰트
//    3.
#pragma pack(pop)


}  // namespace perfkit::_net
