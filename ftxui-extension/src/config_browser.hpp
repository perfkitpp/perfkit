//
// Created by Seungwoo on 2021-09-04.
//
#pragma once
#include <list>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "perfkit/configs.h"
#include "range/v3/algorithm.hpp"
#include "range/v3/view.hpp"
#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"

namespace perfkit {
std::shared_ptr<spdlog::logger> glog();
}

namespace perfkit_ftxui {
using namespace perfkit;
using namespace ftxui;
namespace views = ranges::views;

class config_node_builder {
 private:
  struct _config_data {
    config_ptr content;
  };

 public:
  void _put_new(array_view<std::string_view const> category, config_ptr&& ptr) {
    // initial full key will be used for sort categories.
    if (_subnodes.empty()) { _reference_key = ptr->full_key(); }

    if (category.empty()) {
      if (!_subnodes.empty()) { throw std::invalid_argument(
          fmt::format("A variable name cannot duplicate category name! ({})",
                      ptr->display_key())); }
      _content = std::move(ptr);
    } else {
      if (_content) { throw std::invalid_argument(
          fmt::format("A category name cannot duplicate variable name! ({})",
                      _content->display_key())); }

      auto key = category[0];
      auto it  = _subnodes.find(key);

      if (it == _subnodes.end()) {
        auto [iter, _] = _subnodes.try_emplace(std::string(key));
        it             = iter;
      }

      it->second._put_new(category.subspan(1), std::move(ptr));
    }
  }

  size_t count() {
    if (_content) { return 1; }

    size_t cnt = 0;
    for (auto& item : _subnodes) {
      cnt += item.second.count();
    }

    return cnt;
  }

  std::function<Element()> decorator() {
    if (_content) {
      return [ptr = _content, js = nlohmann::json{}]() mutable {
        ptr->serialize(js);

        switch (js.type()) {
          case nlohmann::detail::value_t::null:
            return color(Color::Red, text("NULL"));
          case nlohmann::detail::value_t::discarded:
            return color(Color::Red, text("--INVALID--"));
          case nlohmann::detail::value_t::object:
            return color(Color::Gold1, text(js.dump().substr(0, 20)));
          case nlohmann::detail::value_t::array:
            return color(Color::DarkViolet, text(js.dump().substr(0, 20)));
          case nlohmann::detail::value_t::string:
            return color(Color::SandyBrown, text(js.dump()));
          case nlohmann::detail::value_t::boolean:
            return color(Color::LightSteelBlue, text(js.dump()));
          case nlohmann::detail::value_t::number_integer:
          case nlohmann::detail::value_t::number_float:
          case nlohmann::detail::value_t::number_unsigned:
          case nlohmann::detail::value_t::binary:
            return color(Color::LightGreen, text(js.dump()));
        }

        return text("content");
      };
    } else {
      auto cnt = count();
      return [cnt] { return color(Color::GrayDark,
                                  text(fmt::format("{} item{}", cnt, cnt > 1 ? "s" : ""))); };
    }
  }

  bool is_content() const { return !!_content; }

  ftxui::Component build(
      std::shared_ptr<std::function<void(bool)>> parent_fold = {}) {
    if (_content) {
      return _build_content_modifier();

    } else if (!_subnodes.empty()) {
      auto subnode_ptr = _subnodes
                         | views::transform([](auto&& c) { return &c; })
                         | ranges::to_vector;

      // for same category hierarchy, compare with full key string.
      ranges::sort(subnode_ptr,
                   [](auto&& a, auto&& b) {
                     return a->second._reference_key < b->second._reference_key;
                   });

      auto subnodes = Container::Vertical({});

      // expose subnodes as individual buttons, which extended/collapsed when clicked.
      for (auto snode : subnode_ptr) {
        auto label_cont    = Container::Vertical({});
        auto state_boolean = std::make_shared<bool>();

        CheckboxOption opts;
        if (snode->second.is_content()) {
          opts.style_checked   = "*";
          opts.style_unchecked = "";
        }

        static auto is_active = [](auto&& container) {
          return container->ChildCount() == 2;
        };
        static auto fold_or_unfold = [](auto&& container, auto&& child, bool en, auto&& state) {
          if (!en && is_active(container)) { container->ChildAt(1)->Detach(); }
          if (en && !is_active(container)) {
            child->OnEvent(Event::F5);
            container->Add(child);
          }

          *state = is_active(container);
        };

        auto fold_fn      = std::make_shared<std::function<void(bool)>>();
        auto node_content = snode->second.build(fold_fn);

        opts.on_change = [node_content, label_cont, state_boolean] {
          fold_or_unfold(label_cont, node_content, !is_active(label_cont), state_boolean);
        };
        *fold_fn = [node_content, label_cont, state_boolean](bool b) {
          fold_or_unfold(label_cont, node_content, b, state_boolean);
        };

        auto label = Checkbox(snode->first, state_boolean.get(), std::move(opts));
        label
            = CatchEvent(label,
                         [parent_fold, fold_fn](Event evt) -> bool {
                           if (evt == Event::Backspace) {
                             (*(parent_fold ? parent_fold : fold_fn))(false);
                           }
                           return false;
                         });

        label_cont->Add(Renderer(label, [label, deco = snode->second.decorator()] {
          return hbox(label->Render(), filler(), text(" "), deco());
        }));
        subnodes->Add(label_cont);
      }

      return Renderer(subnodes,
                      [subnodes] {
                        return hbox(text(" "), subnodes->Render() | xflex);
                      });
    }

    assert(false && "This routine cannot be entered.");
  }

 private:
  Component _build_content_modifier() {
    auto ptr     = std::make_shared<_config_data>();
    ptr->content = _content;

    Component inner;
    auto      proto = _content->serialize();
    switch (proto.type()) {
      case nlohmann::detail::value_t::null:
        return Renderer([] { return color(Color::Red, text("NULL")); });

      case nlohmann::detail::value_t::discarded:
        return Renderer([] { return color(Color::Red, text("--INVALID--")); });

      case nlohmann::detail::value_t::object:
      case nlohmann::detail::value_t::array:
        inner = Renderer([] { return color(Color::GrayDark, text("Temporary")); });
        break;

      case nlohmann::detail::value_t::string: {
        auto str = std::make_shared<std::string>();
        *str     = proto.get<std::string>();
        InputOption opt;
        opt.on_enter = [str, ptr] {
          config_registry::request_update_value(
              std::string{ptr->content->full_key()},
              *str);
        };

        inner = CatchEvent(Input(str.get(), "value", std::move(opt)),
                           [str, ptr](Event evt) {
                             if (evt == Event::F5) { *str = ptr->content->serialize().get<std::string>(); }
                             return false;
                           });

        break;
      }

      case nlohmann::detail::value_t::boolean: {
      }

      case nlohmann::detail::value_t::number_integer:
      case nlohmann::detail::value_t::number_float:
      case nlohmann::detail::value_t::number_unsigned:
      case nlohmann::detail::value_t::binary:
        inner = Renderer([] { return color(Color::GrayDark, text("Temporary")); });
    }

    return Renderer(inner, [inner] {
      return vbox(hbox(text(" "), inner->Render()), separator()) | flex;
    });
  }

 private:
  std::map<std::string, config_node_builder, std::less<>> _subnodes;
  std::string_view                                        _reference_key;
  config_ptr                                              _content;
};

class config_browser : public ftxui::ComponentBase {
 public:
  config_browser() {
    // generate configuration tree from all configs.
    // copy all array then sort with rule
    config_node_builder root;
    for (auto [key, ptr] : perfkit::config_registry::all()) {
      if (ptr->tokenized_display_key().front() == "-") { continue; }  // skip invisible configs
      root._put_new(ptr->tokenized_display_key(), std::move(ptr));
    }

    // from config tree, generate element tree.
    if (perfkit::config_registry::all().empty()) {
      glog()->warn("No available configuration in program !");
      _container = Button("<empty>", [] {});
      return;
    }

    auto built = root.build();
    _container = Renderer(
        built,
        [built] {
          return built->Render() | xflex | yframe;
        });
  }

 public:
  ftxui::Element Render() override {
    return _container->Render();
  }
  bool OnEvent(ftxui::Event event) override {
    return _container->OnEvent(event);
  }
  ftxui::Component ActiveChild() override {
    return _container->ActiveChild();
  }
  bool Focusable() const override {
    return _container->Focusable();
  }
  void SetActiveChild(ComponentBase* child) override {
    return _container->SetActiveChild(child);
  }

 private:
  ftxui::Component _container;
};

}  // namespace perfkit_ftxui