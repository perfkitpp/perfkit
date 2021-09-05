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
  }

 public:
  Element Render() override {
    return _outer->Render();
  }
  bool OnEvent(Event event) override {
    return _outer->OnEvent(event);
  }
  Component ActiveChild() override {
    return _outer->ActiveChild();
  }
  bool Focusable() const override {
    return _outer->Focusable();
  }
  void SetActiveChild(ComponentBase* child) override {
    _outer->SetActiveChild(child);
  }

 private:
  Component _outer;
  tracer* _target;
  tracer::fetched_traces _traces;
  std::future<tracer::async_trace_result> _async_result;
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
  }

 public:
  Element Render() override {
    return _container->Render();
  }
  bool OnEvent(Event event) override {
    return _container->OnEvent(event);
  }
  Component ActiveChild() override {
    return _container->ActiveChild();
  }
  bool Focusable() const override {
    return _container->Focusable();
  }
  void SetActiveChild(ComponentBase* child) override {
    _container->SetActiveChild(child);
  }

 private:
  Component _container;
};

}  // namespace perfkit_ftxui