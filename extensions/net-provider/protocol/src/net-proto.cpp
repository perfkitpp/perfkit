#include "perfkit/_net/net-proto.hpp"

std::string_view perfkit::_net::message_builder::_serialize(
        uint8_t msg, const nlohmann::json& payload) {
  _buf.clear();
  _buf.resize(sizeof(message_header));

  auto* head = reinterpret_cast<message_header*>(&_buf[0]);
  memcpy(head->header, HEADER.data(), sizeof(message_header));
  head->type._value = msg;

  if (not payload.empty()) {
    nlohmann::json::to_bson(payload, _adapter);

    // possibly memory origin was relocated ...
    head               = reinterpret_cast<message_header*>(&_buf[0]);
    head->payload_size = _buf.size() - sizeof(message_header);
  } else {
    head->payload_size = 0;
  }

  return _buf;
}
