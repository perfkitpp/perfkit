#pragma once
#include <string_view>

#include <nlohmann/json.hpp>

#define INTERNAL_PERFKIT_GEN_MARSHAL(type, ...)              \
  void _from_json_m(nlohmann::json const& js) {              \
    _from_json(js, __VA_ARGS__);                             \
  }                                                          \
  void _to_json_m(nlohmann::json& js) const {                \
    _to_json(js, __VA_ARGS__);                               \
  }                                                          \
  friend void from_json(nlohmann::json const& js, type& r) { \
    r._from_json_m(js);                                      \
  }                                                          \
  friend void to_json(nlohmann::json& js, type const& r) {   \
    r._to_json_m(js);                                        \
  }

namespace perfkit::_net {
using json = nlohmann::json;

constexpr std::string_view HEADER = "p=&f4?t";

template <typename... Args_>
void _from_json(nlohmann::json const& js, Args_&... args) {
  size_t Idx_ = 0;
  ((args = js[Idx_++].get_ref<Args_ const&>()), ...);
}

template <typename... Args_>
void _to_json(nlohmann::json& js, Args_&&... args) {
  size_t Idx_ = 0;
  ((js[Idx_++] = std::forward<Args_>(args)), ...);
}

enum class provider_message : uint8_t {
  invalid          = 0,
  register_session = 0x01,
  config_all       = 0x02,
  config_update    = 0x03,
  trace_all        = 0x04,
  trace_image      = 0x05,
  shell_flush      = 0x06,
  shell_suggest    = 0x07,
};

enum class server_message : uint8_t {
  invalid      = 0,
  heartbeat    = 0x81,
  config_fetch = 0x82,  // fetches accumulated updates
  shell_input  = 0x83,
  shell_fetch  = 0x84,
};

#pragma pack(push, 1)
struct message_header {
  char header[7];
  union {
    provider_message session;
    server_message server;
    uint8_t _value;
  } type;
  int32_t payload_size;
};
#pragma pack(pop)

// builds message from type and json payload
struct message_builder {
  std::string_view operator()(provider_message msg, json const& payload) {
    return _serialize(static_cast<uint8_t>(msg), payload);
  }

  std::string_view operator()(server_message msg, json const& payload) {
    return _serialize(static_cast<uint8_t>(msg), payload);
  }

  template <typename Ty_>
  std::string_view operator()(Ty_ const& msg) {
    return _serialize(static_cast<uint8_t>(Ty_::MESSAGE), msg);
  }

  std::string_view get() const noexcept { return _buf; }

 private:
  std::string_view _serialize(uint8_t msg, json const& payload);

 private:
  std::string _buf;
  nlohmann::detail::output_adapter<char> _adapter{_buf};
};

//
struct session_register_info {
  static constexpr auto MESSAGE = provider_message::register_session;

  std::string session_name;
  std::string machine_name;
  int64_t pid;
  int64_t epoch;
  std::string description;

  INTERNAL_PERFKIT_GEN_MARSHAL(
          session_register_info, session_name, machine_name, pid, epoch, description);
};

//
struct shell_flush_chunk {
  static constexpr auto MESSAGE = provider_message::shell_flush;

  size_t sequence_offset;
  std::string data;
};

struct shell_input_line {
  static constexpr auto MESSAGE = server_message::shell_input;
  std::string content;
};

struct shell_suggest_line {
  static constexpr auto MESSAGE = provider_message::shell_suggest;
  std::string content;
};

// -- Manipulating Configs --
// config_all: 모든 컨피겨레이션 서술자를 내보냅니다.
//  이 때, 각 디렉터리별로 오브젝트 계층 정렬하기
//    0. 이름
//    1. 키 값에 기반한 해시
//    2. 모든 어트리뷰트
//    3. 설명
//  서버 사이드에서는 모든 변경사항 병합 및 관리
struct config_desc_pack {
  static constexpr auto MESSAGE = provider_message::config_all;
};

struct config_update_pack {
  static constexpr auto MESSAGE = provider_message::config_update;
  std::map<uint64_t, json> updates;
};

}  // namespace perfkit::_net
