//
// Created by Seungwoo on 2021-09-04.
//
#pragma once
#include <charconv>
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
            return color(Color::Gold1, text(js.dump()));
          case nlohmann::detail::value_t::array:
            return color(Color::DarkViolet, text(js.dump()));
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
      std::shared_ptr<std::function<void(bool)>> parent_fold = nullptr) {
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

      auto subnodes    = Container::Vertical({});
      auto fold_fn_all = std::make_shared<std::function<void(bool)>>([](bool) {});

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
            child->TakeFocus();
          }

          *state = is_active(container);
        };

        auto fold_fn         = std::make_shared<std::function<void(bool)>>();
        auto node_content    = snode->second.build(fold_fn);
        auto subnode_fold_fn = *fold_fn;

        opts.on_change = [node_content, label_cont, state_boolean] {
          fold_or_unfold(label_cont, node_content, !is_active(label_cont), state_boolean);
        };
        *fold_fn = [node_content, label_cont, state_boolean](bool b) {
          fold_or_unfold(label_cont, node_content, b, state_boolean);
        };
        *fold_fn_all = [parent_fold, fold_fn, subnode_fold_fn, fn_all = *fold_fn_all](bool b) {
          subnode_fold_fn && (subnode_fold_fn(b), 1);
          fn_all(b);
          (*fold_fn)(b);
          parent_fold && ((*parent_fold)(b), 1);
        };

        auto label = Checkbox(snode->first, state_boolean.get(), std::move(opts));
        label
            = CatchEvent(label,
                         [fold_fn_all](Event const& evt) -> bool {
                           if (evt == Event::Backspace) {
                             (*fold_fn_all)(false);
                           }
                           return false;
                         });

        label_cont->Add(Renderer(label, [label, deco = snode->second.decorator(),
                                         is_content = snode->second.is_content()] {
          auto label_render = label->Render();
          if (is_content) { label_render = color(Color::Cyan, label_render); }
          return hbox(label_render, filler(), text(" "), deco());
        }));

        subnodes->Add(
            Renderer(label_cont, [label_cont] { return hbox(text("  "), label_cont->Render() | flex); }));
      }

      parent_fold && (*parent_fold = *fold_fn_all, 1);
      return Renderer(subnodes,
                      [subnodes] {
                        return (subnodes->Render() | xflex);
                      });
    }

    assert(false && "This routine cannot be entered.");
  }

 private:
  struct _json_editor_builder {
    config_ptr cfg;
    bool allow_schema_modification;
    nlohmann::json rootobj;
    std::function<void()> on_change;

    Component _iter(nlohmann::json* obj) {
      switch (obj->type()) {
        case nlohmann::detail::value_t::null:
          break;

        case nlohmann::detail::value_t::object: {
          auto outer = Container::Vertical({});
          for (auto it = (*obj).begin(); it != (*obj).end(); ++it) {
            auto inner_value = _iter(&it.value());

            auto inner = Renderer(inner_value,
                                  [this,
                                   key = text(fmt::format("\"{}\":", it.key())),
                                   inner_value] {
                                    return hbox(
                                               color(Color::Yellow, key),
                                               text(" {"), inner_value->Render(), text("}"))
                                           | flex;
                                  });

            outer->Add(inner);
          }

          return Renderer(outer, [outer] { return outer->Render() | flex; });
        }

        case nlohmann::detail::value_t::array: {
          auto outer = Container::Vertical({});

          for (size_t i = 0; i < obj->size(); ++i) {
            auto inner_value = _iter(&(*obj)[i]);

            auto inner = Renderer(inner_value,
                                  [this,
                                   key = text(fmt::format("[{}]:", i)),
                                   inner_value] {
                                    return hbox(
                                        color(Color::Violet, key),
                                        text(" {"), inner_value->Render(), text("}"));
                                  });
            outer->Add(inner);
          }
          return Renderer(outer, [outer] { return outer->Render() | flex; });
        }

        case nlohmann::detail::value_t::string: {
          InputOption opt;
          opt.on_enter        = on_change;
          opt.cursor_position = 1 << 20;

          return Input(obj->get_ptr<std::string*>(), "value", std::move(opt));
        }

        case nlohmann::detail::value_t::boolean: {
          CheckboxOption opt;
          opt.style_checked   = "true ";
          opt.style_unchecked = "false";
          opt.on_change       = on_change;

          return Checkbox("<>", obj->get_ptr<bool*>(), std::move(opt));
        }

        case nlohmann::detail::value_t::number_integer:
        case nlohmann::detail::value_t::number_unsigned:
        case nlohmann::detail::value_t::number_float: {
          auto pwstr = std::make_shared<std::wstring>();
          *pwstr     = ftxui::to_wstring(obj->dump());
          InputOption opt;
          opt.cursor_position = 1 << 20;

          opt.on_enter = [pwstr, this, obj] {
            double value = 0;
            auto str     = ftxui::to_string(*pwstr);

            try {
              auto result = std::stod(str);
              *obj        = result;
              on_change();
            } catch (std::invalid_argument&) {}
          };

          return Input(pwstr.get(), "value", std::move(opt));
        }

        case nlohmann::detail::value_t::binary:
        case nlohmann::detail::value_t::discarded:
          break;
      }

      ButtonOption nulloption;
      nulloption.border = false;
      return Button(
          obj->type_name(), [] {}, nulloption);
    }
  };

  Component _build_content_modifier() {
    auto cfg = _content;

    Component inner;
    auto proto     = _content->serialize();
    auto ptr       = std::make_shared<_json_editor_builder>();
    ptr->on_change = [cfg, ptr] { cfg->request_modify(ptr->rootobj); };
    ptr->cfg       = cfg;
    ptr->rootobj   = proto;

    ptr->allow_schema_modification = false;

    inner = Container::Vertical({ptr->_iter(&ptr->rootobj)});
    inner = CatchEvent(inner,
                       [inner, ptr](Event evt) {
                         if (evt == Event::F5) {
                           ptr->rootobj = ptr->cfg->serialize();
                           inner->DetachAllChildren();
                           inner->Add({ptr->_iter(&ptr->rootobj)});
                         }
                         return false;
                       });
    inner = Renderer(inner, [inner] { return inner->Render() | flex; });

    return Renderer(inner, [inner] {
      return vbox(hbox(text(" "), inner->Render()), color(Color::Cyan, separator())) | flex;
    });
  }

 private:
  std::map<std::string, config_node_builder, std::less<>> _subnodes;
  std::string_view _reference_key;
  config_ptr _content;
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
          return (built->Render() | xflex | yframe);
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