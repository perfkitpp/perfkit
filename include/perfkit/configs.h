#pragma once
#include "perfkit/detail/configs.hpp"

namespace perfkit::_internal {
std::string INDEXER_STR(int order);
}

#define PERFKIT_CONFIG_REGISTRY(Name) static inline auto& Name = perfkit::config_registry::create()

#define INTERNAL_PERFKIT_STRINGFY_2(A) #A
#define INTERNAL_PERFKIT_STRINGFY(A)   INTERNAL_PERFKIT_STRINGFY_2(A)
#define INTERNAL_PERFKIT_INDEXER       "+" + ::perfkit::_internal::INDEXER_STR(__COUNTER__) + "|"  // max 99999

#define PERFKIT_FILEMARKER __FILE__ ":" INTERNAL_PERFKIT_STRINGFY(__LINE__) " " INTERNAL_PERFKIT_STRINGFY(__func__) "(): "

// HELPER MACROS
#define PERFKIT_CATEGORY(category)                                                   \
  namespace category {                                                               \
  ::std::string _perfkit_INTERNAL_CATNAME() {                                        \
    return "";                                                                       \
  }                                                                                  \
  ::std::string _perfkit_INTERNAL_CATNAME_2() {                                      \
    return _perfkit_INTERNAL_CATNAME();                                              \
  }                                                                                  \
  ::perfkit::config_registry& _registry() {                                          \
    static auto inst = ::perfkit::config_registry::create(#category);                \
    return *inst;                                                                    \
  }                                                                                  \
  [[deprecated]] ::perfkit::config_registry& root_registry() { return _registry(); } \
  ::perfkit::config_registry& registry() { return _registry(); }                     \
  }                                                                                  \
  namespace category

#define PERFKIT_SUBCATEGORY(category)                                                               \
  namespace category {                                                                              \
  static inline auto _perfkit_INTERNAL_CATNAME_BEFORE = &_perfkit_INTERNAL_CATNAME;                 \
  [[deprecated]] ::perfkit::config_registry& root_registry() { return _registry(); }                \
  ::perfkit::config_registry& registry() { return _registry(); }                                    \
  ::std::string _perfkit_INTERNAL_CATNAME() {                                                       \
    return category::_perfkit_INTERNAL_CATNAME_BEFORE() + INTERNAL_PERFKIT_INDEXER + #category "|"; \
  }                                                                                                 \
  ::std::string _perfkit_INTERNAL_CATNAME_2() {                                                     \
    return _perfkit_INTERNAL_CATNAME();                                                             \
  }                                                                                                 \
  }                                                                                                 \
  namespace category

#define PERFKIT_CONFIGURE(name, default_value)                                                   \
  auto name = perfkit::configure(registry(),                                                     \
                                 _perfkit_INTERNAL_CATNAME_2() + INTERNAL_PERFKIT_INDEXER #name, \
                                 default_value)

#define PERFKIT_FORWARD_CATEGORY(hierarchy)                   \
  namespace hierarchy {                                       \
  ::std::string _perfkit_INTERNAL_CATNAME_2();                \
  [[deprecated]] ::perfkit::config_registry& root_registry(); \
  ::perfkit::config_registry& registry();                     \
  }                                                           \
  namespace hierarchy

#define PERFKIT_FORWARD_ROOT_CATEGORY(hierarchy) \
  namespace hierarchy {                          \
  ::std::string _perfkit_INTERNAL_CATNAME();     \
  ::perfkit::config_registry& _registry();       \
  ::perfkit::config_registry& registry();        \
  }                                              \
  namespace hierarchy
