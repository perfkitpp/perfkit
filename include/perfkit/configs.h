#pragma once
#include "perfkit/detail/configs.hpp"

namespace perfkit::_internal {
std::string INDEXER_STR(int order);

struct category_template_base {
  virtual ~category_template_base()                                = default;
  virtual std::shared_ptr<perfkit::config_registry> _rg() noexcept = 0;
};
}  // namespace perfkit::_internal

#define PERFKIT_CONFIG_REGISTRY(Name) static inline auto& Name = perfkit::config_registry::create()

#define INTERNAL_PERFKIT_STRINGFY_2(A) #A
#define INTERNAL_PERFKIT_STRINGFY(A)   INTERNAL_PERFKIT_STRINGFY_2(A)
#define INTERNAL_PERFKIT_INDEXER       "+" + ::perfkit::_internal::INDEXER_STR(__COUNTER__) + "|"  // max 99999

#define PERFKIT_FILEMARKER __FILE__ ":" INTERNAL_PERFKIT_STRINGFY(__LINE__) " " INTERNAL_PERFKIT_STRINGFY(__func__) "(): "

// HELPER MACROS
#define PERFKIT_CATEGORY(category)                                                   \
  namespace category {                                                               \
                                                                                     \
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
                                                                                     \
  bool update() { return _registry().update(); }                                     \
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

#define PERFKIT_CONFIGURE(name, ...)                                                             \
  ::perfkit::config<::perfkit::_cvt_ty<decltype(__VA_ARGS__)>>                                   \
          name                                                                                   \
          = ::perfkit::configure(registry(),                                                     \
                                 _perfkit_INTERNAL_CATNAME_2() + INTERNAL_PERFKIT_INDEXER #name, \
                                 __VA_ARGS__)

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
  bool update();                                 \
  }                                              \
  namespace hierarchy

#define INTERNAL_PERFKIT_T_CATEGORY_body(name)                        \
 private:                                                             \
  using _internal_super = name;                                       \
  std::shared_ptr<::perfkit::config_registry> _perfkit_INTERNAL_RG;   \
  static std::string _category_name() { return ""; }                  \
                                                                      \
  std::shared_ptr<perfkit::config_registry> _rg() noexcept override { \
    return _perfkit_INTERNAL_RG;                                      \
  }                                                                   \
                                                                      \
 public:                                                              \
  explicit name(std::string s) : _perfkit_INTERNAL_RG(                \
          ::perfkit::config_registry::create(std::move(s))) {}        \
  ::perfkit::config_registry* operator->() { return _perfkit_INTERNAL_RG.get(); }

#define INTERNAL_PERFKIT_T_SUBCATEGORY_body(varname)                \
 private:                                                           \
  static std::string _category_name() {                             \
    return _category_prev_##varname() + #varname "|";               \
  }                                                                 \
                                                                    \
  ::perfkit::config_registry* _perfkit_INTERNAL_RG;                 \
                                                                    \
 public:                                                            \
  template <typename TT_>                                           \
  explicit varname##_(TT_* _super)                                  \
          : _perfkit_INTERNAL_RG(&*_super->_perfkit_INTERNAL_RG) {} \
                                                                    \
 private:                                                           \
  using _internal_super = varname##_;                               \
                                                                    \
 public:

#define PERFKIT_T_CATEGORY(varname, ...)                          \
  struct varname : ::perfkit::_internal::category_template_base { \
    INTERNAL_PERFKIT_T_CATEGORY_body(varname);                    \
    __VA_ARGS__;                                                  \
  }

#define PERFKIT_T_SUBCATEGORY(varname, ...)                                  \
  static std::string _category_prev_##varname() { return _category_name(); } \
  struct varname##_ {                                                        \
    INTERNAL_PERFKIT_T_SUBCATEGORY_body(varname);                            \
    __VA_ARGS__;                                                             \
  } varname{this};

#define PERFKIT_T_CONFIGURE(name, ...)               \
  ::perfkit::config<                                 \
          ::perfkit::_cvt_ty<decltype(__VA_ARGS__)>> \
          name = ::perfkit::configure(               \
                  *_perfkit_INTERNAL_RG,             \
                  _category_name() + #name,          \
                  __VA_ARGS__)

#define PERFKIT_T_EXPAND_CATEGORY(varname, ...)                                     \
  struct varname : ::perfkit::_internal::category_template_base {                   \
   private:                                                                         \
    using _internal_super = varname;                                                \
    std::shared_ptr<::perfkit::config_registry> _perfkit_INTERNAL_RG;               \
    static std::string _category_name() { return #varname; }                        \
                                                                                    \
    std::shared_ptr<perfkit::config_registry> _rg() noexcept override {             \
      return _perfkit_INTERNAL_RG;                                                  \
    }                                                                               \
                                                                                    \
   public:                                                                          \
    explicit varname(::perfkit::_internal::category_template_base* other)           \
            : _perfkit_INTERNAL_RG(other->_rg()) {}                                 \
    ::perfkit::config_registry* operator->() { return _perfkit_INTERNAL_RG.get(); } \
    bool update() { return _perfkit_INTERNAL_RG->update(); }                        \
                                                                                    \
   public:                                                                          \
    __VA_ARGS__;                                                                    \
  };
