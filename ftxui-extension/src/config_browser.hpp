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

  std::function<Element()> decorate() {
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

  ftxui::Component build() {
    if (_content) {
      auto ptr     = std::make_shared<_config_data>();
      ptr->content = _content;

      return Renderer([ptr] {
        return text(std::string(ptr->content->full_key()));
      });
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
        auto node_content  = snode->second.build();
        auto state_boolean = std::make_shared<bool>();

        CheckboxOption opts;
        if (snode->second.is_content()) {
          opts.style_checked   = "*";
          opts.style_unchecked = "";
        }

        opts.on_change = [node_content, label_cont, state_boolean] {
          if (label_cont->ChildCount() == 1) {
            label_cont->Add(node_content);
            *state_boolean = true;
          } else {
            label_cont->ChildAt(1)->Detach();
            *state_boolean = false;
          }
        };

        auto label = Checkbox(snode->first, state_boolean.get(), opts);

        label_cont->Add(
            Renderer(label, [label, deco = snode->second.decorate()] {
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