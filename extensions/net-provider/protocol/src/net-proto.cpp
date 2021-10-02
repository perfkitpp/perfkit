#include "perfkit/_net/net-proto.hpp"

std::string_view perfkit::_net::message_builder::_serialize(
        uint8_t msg, const nlohmann::json& payload) {
  std::string output;
  nlohmann::json::to_bson(payload, output);
  return _buf;
}
