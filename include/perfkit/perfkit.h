//
// Created by Seungwoo on 2021-08-25.
//
#pragma once
#include <spdlog/fwd.h>

#include <iostream>
#include <perfkit/detail/configs.hpp>
#include <perfkit/detail/tracer.hpp>

namespace perfkit {
std::shared_ptr<spdlog::logger> glog();

bool import_options(std::string_view src);
bool export_options(std::string_view dstpath);

namespace _internal {
std::string INDEXER_STR(int order);
}
}  // namespace perfkit

#define PERFKIT_CONFIG_REGISTRY(Name) static inline auto& Name = perfkit::config_registry::create()
#define PERFKIT_MESSAGE_BLOCK         static inline perfkit::tracer

#define INTERNAL_PERFKIT_STRINGFY_2(A) #A
#define INTERNAL_PERFKIT_STRINGFY(A)   INTERNAL_PERFKIT_STRINGFY_2(A)
#define INTERNAL_PERFKIT_INDEXER       "+" + ::perfkit::_internal::INDEXER_STR(__COUNTER__) + "|"  // max 99999

#define PERFKIT_FILEMARKER __FILE__ ":" INTERNAL_PERFKIT_STRINGFY(__LINE__) " " INTERNAL_PERFKIT_STRINGFY(__func__) "(): "

// HELPER MACROS
#define PERFKIT_CATEGORY(category)                                   \
  namespace category {                                               \
  ::std::string _perfkit_INTERNAL_CATNAME() {                        \
    return INTERNAL_PERFKIT_INDEXER #category "|";                   \
  }                                                                  \
  ::std::string _perfkit_INTERNAL_CATNAME_2() {                      \
    return _perfkit_INTERNAL_CATNAME();                              \
  }                                                                  \
  ::perfkit::config_registry& registry() {                           \
    static auto& inst = ::perfkit::config_registry::create();        \
    return inst;                                                     \
  }                                                                  \
  ::perfkit::config_registry& root_registry() { return registry(); } \
  }                                                                  \
  namespace category

#define PERFKIT_SUBCATEGORY(category)                                                                             \
  namespace category {                                                                                            \
  static inline auto          _perfkit_INTERNAL_CATNAME_BEFORE = &_perfkit_INTERNAL_CATNAME;                      \
  ::perfkit::config_registry& root_registry() { return registry(); }                                              \
  ::std::string               _perfkit_INTERNAL_CATNAME() {                                                       \
    return category::_perfkit_INTERNAL_CATNAME_BEFORE() + INTERNAL_PERFKIT_INDEXER + #category "|"; \
  }                                                                                                               \
  ::std::string _perfkit_INTERNAL_CATNAME_2() {                                                                   \
    return _perfkit_INTERNAL_CATNAME();                                                                           \
  }                                                                                                               \
  }                                                                                                               \
  namespace category

#define PERFKIT_CONFIGURE(name, default_value)                                                   \
  auto name = perfkit::configure(root_registry(),                                                \
                                 _perfkit_INTERNAL_CATNAME_2() + INTERNAL_PERFKIT_INDEXER #name, \
                                 default_value)

#define PERFKIT_FORWARD_CATEGORY(hierarchy)                  \
  namespace hierarchy {                                      \
  ::std::string               _perfkit_INTERNAL_CATNAME_2(); \
  ::perfkit::config_registry& root_registry();               \
  }                                                          \
  namespace hierarchy
