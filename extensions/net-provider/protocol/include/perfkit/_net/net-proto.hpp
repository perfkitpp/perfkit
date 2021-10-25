#pragma once
#include <string_view>

#include <nlohmann/json.hpp>

#define INTERNAL_PERFKIT_GEN_MARSHAL(type, ...)              \
  void _from_json_m(nlohmann::json const& js) {              \
    _from_json(js, ##__VA_ARGS__);                           \
  }                                                          \
  void _to_json_m(nlohmann::json& js) const {                \
    _to_json(js, ##__VA_ARGS__);                             \
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
void _from_json(nlohmann::json const& obj, Args_&... args) {
  size_t Idx_ = 0;
  auto& js    = obj["content"];  // To serialize as BSON, create root object
  ((args = js[Idx_++].get<Args_>()), ...);
}

template <typename... Args_>
void _to_json(nlohmann::json& obj, Args_&&... args) {
  size_t Idx_ = 0;
  auto& js    = obj["content"];
  ((js[Idx_++] = std::forward<Args_>(args)), ...);
}

enum class provider_message : uint8_t {
  invalid             = 0,
  register_session    = 0x01,
  image_reply         = 0x02,
  session_flush_reply = 0x07,
  shell_suggest       = 0x08,
  heartbeat           = 0x0F,
};

enum class server_message : uint8_t {
  invalid               = 0,
  trace_interaction     = 0x81,
  image_interaction     = 0x82,
  config_interaction    = 0x83,
  shell_enter           = 0x84,
  session_flush_request = 0x85,
  heartbeat             = 0x8F,
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
          session_register_info,
          session_name,
          machine_name,
          pid,
          epoch,
          description);
};

struct config_entity_descriptor {
  std::string full_key;
  std::string display_key;
  nlohmann::json metadata;

  INTERNAL_PERFKIT_GEN_MARSHAL(
          config_entity_descriptor,
          full_key,
          metadata);
};

struct config_update_descriptor {
  std::string full_key;
  nlohmann::json data;

  INTERNAL_PERFKIT_GEN_MARSHAL(
          config_update_descriptor,
          full_key,
          data);
};

struct config_registry_descriptor {
  std::string name;
  std::vector<config_entity_descriptor> entities;

  INTERNAL_PERFKIT_GEN_MARSHAL(
          config_registry_descriptor,
          name,
          entities);
};

//
struct session_flush_chunk {
  static constexpr auto MESSAGE = provider_message::session_flush_reply;

  int64_t sequence;
  std::string shell_content;

  std::vector<config_registry_descriptor> config_category_new;
  std::vector<std::string> config_category_deleted;

  INTERNAL_PERFKIT_GEN_MARSHAL(
          session_flush_chunk,
          sequence,
          shell_content,
          config_category_new,
          config_category_deleted);
};

struct shell_input_line {
  static constexpr auto MESSAGE = server_message::shell_enter;
  int64_t request_id;
  bool is_suggest;
  std::string content;

  INTERNAL_PERFKIT_GEN_MARSHAL(shell_input_line, request_id, is_suggest, content);
};

struct shell_suggest_reply {
  static constexpr auto MESSAGE = provider_message::shell_suggest;
  int64_t request_id;
  std::string content;
  std::vector<nlohmann::json> suggest_words;

  INTERNAL_PERFKIT_GEN_MARSHAL(shell_suggest_reply, request_id, content, suggest_words);
};

// -- Manipulating Traces --
// 1. Provider -> Server로 Group List 송신
// 2. Client -> Server로 Group Fetch 호출
//    서버는 일단 None 반환 (항상 즉시 반환)
// 3. Server -> Provider로 Group Open 요청
// 4. Provider -> Server 틱마다 Trace 업로드

// Image Handling
// 1. Client -> Server로 Image Load 요청
// 2. Server -> Provider에 이미지 요청
// 3. Provider -> Server에 최신 이미지 Jpeg + 시퀀스 인덱스 인코딩해 전달(시퀀스 최신이면 즉시응답)
// 4. Server -> Client에 해당 이미지 포워드 및, 서버는 해당 이미지 캐싱

// -- Manipulating Configs --
// config_all: 모든 컨피겨레이션 서술자를 내보냅니다.
//  이 때, 각 디렉터리별로 오브젝트 계층 정렬하기
//    0. 이름
//    1. 키 값에 기반한 해시
//    2. 모든 어트리뷰트
//    3. 설명
//  서버 사이드에서는 모든 변경사항 병합 및 관리

}  // namespace perfkit::_net
