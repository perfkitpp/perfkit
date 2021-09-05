//
// Created by Seungwoo on 2021-09-05.
//
#pragma once
#include "ftxui/component/component.hpp"
#include "perfkit/traces.h"

namespace perfkit_ftxui {
using namespace ftxui;
using namespace perfkit;

class tracer_instance_browser : public ComponentBase {
 public:
  explicit tracer_instance_browser(tracer* target)
      : _target(target) {
    {
      CheckboxOption opts;
      opts.style_checked   = "";
      opts.style_unchecked = "";

      _title = Checkbox(_target->name(), &_is_fetching, opts);
      Add(_title);
    }
    {
      _outer = Container::Vertical({});
    }
  }

 public:
  Element Render() override {
    auto title = hbox(spinner(15, poll_count), _title->Render(), text(fmt::format("{}", poll_count)));

    if (_is_fetching) {
      return vbox(title | color(Color::DarkGreen), hbox(text("  "), _outer));
    } else {
      return title;
    }
  }

  bool OnEvent(Event event) override {
    if (event == EVENT_POLL) {
      // Request trace result fetch periodically.
      if (_async_result.valid()) {
        if (_async_result.wait_for(0ms) == std::future_status::ready) {
          auto result = _async_result.get();
          _traces.clear(), result.copy_sorted(_traces);
          _async_result = {};
        }
      } else if (_is_fetching) {
        _async_result = _target->async_fetch_request();
      }

      // Increase poll index. This is used to visualize spinner
      ++poll_count;
      return false;
    }
    return ComponentBase::OnEvent(event);
  }

 private:
  Component _title;
  Component _outer;

  bool _is_fetching = false;
  std::map<uint64_t, bool> is_folded;

  tracer* _target;
  tracer::fetched_traces _traces;
  tracer::future_result _async_result;
  size_t poll_count = 0;
};

// 1. 루트 트레이스 -> 각 tracer 인스턴스에 대한 fetch content 활성 제어
// 2. 서브 트레이스 -> 트리 형태로 구성, 매 fetch마다 모든 프레임 재구성, subscrption 제어 및 fold 제어
//    1. fold 제어 -> 인스턴스에 hash 테이블 구성 ... 만나면 해당 높이 멈추기
//    2. subscription 제어 -> 대상 엔터티에 단순히 false/true 토글 ... 자동으로 대상에서 값 제외하고 보낸다
class trace_browser : public ComponentBase {
 public:
  trace_browser() {
    _container = Container::Vertical({});

    // iterate all tracers and create tracer root object
    for (auto item : tracer::all()) {
      _container->Add(std::make_shared<tracer_instance_browser>(item));
    }

    Add(event_dispatcher(_container));
  }

 public:
  Element Render() override {
    return _container->Render() | flex | frame;
  }

 private:
  Component _container;
};

}  // namespace perfkit_ftxui