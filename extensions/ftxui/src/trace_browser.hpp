/*******************************************************************************
 * MIT License
 *
 * Copyright (c) 2021. Seungwoo Kang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * project home: https://github.com/perfkitpp
 ******************************************************************************/

//
// Created by Seungwoo on 2021-09-05.
//
#pragma once
#include "ftxui/component/component.hpp"
#include "perfkit/traces.h"
#include "range/v3/algorithm.hpp"

namespace perfkit_ftxui {
using namespace ftxui;
using namespace perfkit;

class tracer_instance_browser : public ComponentBase
{
   public:
    explicit tracer_instance_browser(tracer* target, std::shared_ptr<if_subscriber> monitor)
            : _target(target), _monitor(std::move(monitor))
    {
        {
            CheckboxOption opts;
            opts.style_checked   = "";
            opts.style_unchecked = "";

            opts.on_change
                    = [this] {
                          if (_is_fetching && _container->ChildCount() == 1) { _container->Add(_outer); }
                          if (!_is_fetching && _container->ChildCount() == 2) { _container->ChildAt(1)->Detach(); }
                      };

            _title = Checkbox(_target->name(), &_is_fetching, opts);
        }
        {
            _outer = Container::Vertical({});
        }
        _container = Container::Vertical({_title, _outer});
        Add(_container);
    }

   public:
    Element Render() override
    {
        auto title = hbox(spinner(15, _latest_fence), text(" "), _title->Render());
        Pixel sep;
        sep.character = ".";
        sep.dim       = true;

        if (_is_fetching)
        {
            return vbox(
                    separator(sep),
                    hbox(title | ftxui::color(Color::DarkGreen),
                         filler(),
                         text(std::to_string(_latest_fence)) | ftxui::color(Color::GrayDark)),
                    (_outer->Render() | flex));
        }
        else
        {
            return vbox(separator(sep), title | ftxui::color(Color::GrayDark));
        }
    }

    bool OnEvent(Event event) override
    {
        if (event == EVENT_POLL)
        {
            // Request trace result fetch periodically.
            if (_async_result.valid())
            {
                if (_async_result.wait_for(0ms) == std::future_status::ready)
                {
                    auto result = _async_result.get();
                    _traces.clear(), result.copy_sorted(_traces);
                    _async_result = {};

                    _regenerate();
                }
            }

            if (!_async_result.valid() && _is_fetching)
            {
                _target->async_fetch_request(&_async_result);
            }

            // Increase poll index. This is used to visualize spinner
            return false;
        }
        else if (event == Event::Backspace)
        {
            _title->TakeFocus();
            return true;
        }
        else
        {
            return _container->OnEvent(event);
        }
    }

   private:
    struct _data_label : ComponentBase
    {
       public:
        explicit _data_label(tracer_instance_browser* owner)
                : _owner(owner)
        {
            CheckboxOption opts;
            opts.on_change       = [this] { _do_refresh(); };
            opts.style_checked   = "<";
            opts.style_unchecked = "|";

            CheckboxOption opts_subcr;
            opts_subcr.on_change       = opts.on_change;
            opts_subcr.style_checked   = "";
            opts_subcr.style_unchecked = "";

            _control_toggle = Container::Horizontal({
                    Checkbox("<|", &_folding, opts),
                    Checkbox("(+)", &_subscribing, opts_subcr),
            });
            Add(_control_toggle);
        }

        Element Render() override
        {
            auto name  = text(_c_name);
            auto value = text(_c_value);

            if (_data.fence < _owner->_latest_fence)
            {
                name = name | ftxui::color(Color::GrayDark);
            }
            else if (_data.data.index() == 0)
            {
                name = name | ftxui::color(Color::Cyan) | bold;
            }
            else
            {
                //        name = name ;
            }

            switch (_data.data.index())
            {
                case 0:  //<clock_type::duration,
                    value = hbox(value | ftxui::color(Color::Violet), text(" [ms]") | dim);
                    break;

                case 1:  // int64_t,
                case 2:  // double,
                    value = value | ftxui::color(Color::GreenLight);
                    break;

                case 4:  // bool,
                    value = value | ftxui::color(Color::BlueLight);
                    break;

                case 3:  // std::string,
                    value = value | ftxui::color(Color::Yellow);
                    break;

                case 5:  // std::any;
                    value = value | underlined;
                    break;

                default:
                    break;
            }

            auto hrange = hbox(name, filler(), value, text(" "));

            auto ret = hbox(text(std::string{}.append(_data.hierarchy.size() * 2, ' ')),
                            _control_toggle->Render(),
                            text(" "), (_data.subscribing() ? hrange | inverted : hrange) | flex)
                     | flex;
            return ret;
        }

        void configure(tracer::trace&& tr, int mod)
        {
            _data        = std::move(tr);
            _folding     = mod & 1;
            _subscribing = mod & (1 << 1);

            {
                _c_name.clear();
                if (_data.data.index() == 0)
                {
                    _c_name.append("[");
                }
                else
                {
                    _c_name.append(" ");
                }

                _c_name.append(_data.key.begin(), _data.key.end());
                if (_data.data.index() == 0) { _c_name.append("]"); }
            }

            _data.dump_data(_c_value);

            _do_refresh();
        }

        void _do_refresh()
        {
            if (_folding) { _owner->_folded_entities.insert(_data.hash); }
            if (!_folding) { _owner->_folded_entities.erase(_data.hash); }

            if (_data.subscribing() != _subscribing)
            {
                _data.subscribe(_subscribing);
                if (!_subscribing && _owner->_monitor) { _owner->_monitor->on_end(_owner->_build_param(_data)); }
            }
            _owner->_cached_modes[_data.hash] = _folding + (_subscribing << 1);
        }

       public:
        std::string _c_name;
        std::string _c_value;

        tracer_instance_browser* _owner;
        Component _control_toggle;
        tracer::trace _data;

        bool _folding     = false;
        bool _subscribing = false;
    };

    void _regenerate()
    {
        _reserve_pool(_traces.size());
        _outer->DetachAllChildren();

        array_view<std::string_view> skipping = {};

        size_t max_fence = 0;
        for (auto& trace : _traces)
        {
            max_fence = std::max(max_fence, trace.fence);

            if (!skipping.empty() && _one_is_root_of(skipping, trace.hierarchy))
            {
                continue;
            }
            else
            {
                // order is always guaranteed.
                skipping = {};
            }

            auto label        = _assign();
            auto [it, is_new] = _cached_modes.try_emplace(trace.hash, 0);
            auto manip_mod    = &it->second;
            if (is_new) { _folded_entities.emplace(trace.hash); }

            if (_is_folded(trace.hash))
            {  // check folded subnode
                skipping = trace.hierarchy;
            }

            auto subscribing          = trace.subscribing();
            bool const update_subscr  = subscribing && trace.fence > _latest_fence;
            bool const trigger_subscr = subscribing && *manip_mod < 2;

            if ((update_subscr || trigger_subscr) && _monitor)
            {
                // update subscription
                auto keep_subscr = _monitor->on_update(_build_param(trace), trace.data);
                if (!keep_subscr) { trace.subscribe(false), subscribing = false; }
            }

            *manip_mod = subscribing ? *manip_mod | 2 : *manip_mod & 1;  // refresh mod by external condition
            label->configure(std::move(trace), *manip_mod);
        }

        _latest_fence = max_fence;
    }

    if_subscriber::update_param_type _build_param(tracer::trace const& data)
    {
        if_subscriber::update_param_type arg;
        arg.block_name = _target->name();
        arg.name       = data.key;
        arg.hash       = data.hash;
        arg.hierarchy  = data.hierarchy;
        return arg;
    }

    void _reserve_pool(size_t s)
    {
        while (_pool.size() < s)
        {
            _pool.emplace_back(std::make_shared<_data_label>(this));
        }
    }

    bool _is_folded(uint64_t hash) const
    {
        auto it = _folded_entities.find(hash);
        return it != _folded_entities.end();
    }

    auto _assign() -> std::shared_ptr<_data_label>
    {
        auto ret = _pool[_outer->ChildCount()];
        return _outer->Add(ret), ret;
    }

    static bool _one_is_root_of(
            array_view<std::string_view> root,
            array_view<std::string_view> sub)
    {
        if (root.size() >= sub.size()) { return false; }
        return std::equal(root.begin(), root.end(), sub.begin());
    }

   private:
    Component _container;
    Component _title;
    Component _outer;

    bool _is_fetching = false;
    std::set<uint64_t> _folded_entities;
    std::map<uint64_t, int> _cached_modes;

    std::vector<std::shared_ptr<_data_label>> _pool;

    tracer* _target;
    tracer::fetched_traces _traces;
    tracer::future_result _async_result;
    size_t _latest_fence = 0;

    std::shared_ptr<if_subscriber> _monitor;
};

// 1. 루트 트레이스 -> 각 tracer 인스턴스에 대한 fetch content 활성 제어
// 2. 서브 트레이스 -> 트리 형태로 구성, 매 fetch마다 모든 프레임 재구성, subscrption 제어 및 fold 제어
//    1. fold 제어 -> 인스턴스에 hash 테이블 구성 ... 만나면 해당 높이 멈추기
//    2. subscription 제어 -> 대상 엔터티에 단순히 false/true 토글 ... 자동으로 대상에서 값 제외하고 보낸다
class trace_browser : public ComponentBase
{
   public:
    explicit trace_browser(std::shared_ptr<if_subscriber> m)
    {
        _container = Container::Vertical({});

        // iterate all tracers and create tracer root object
        for (auto item : tracer::all())
        {
            _container->Add(std::make_shared<tracer_instance_browser>(item, m));
        }

        Add(event_dispatcher(_container));
    }

   public:
    Element Render() override
    {
        return _container->Render() | flex | frame;
    }

   private:
    Component _container;
};

}  // namespace perfkit_ftxui