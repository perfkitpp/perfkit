#pragma once
#include "window.hpp"

namespace perfkit::gui::imgui
{
/**
 * graphics context
 *
 * usage: inside of gui::instance::try_render's handler:
 * \code
 *          [&](buffer* buf) {
 *            context.new_frame(buf);
 *            if(context.window("main")) {
 *              ...;
 *            }
 *
 *            // window name is used for lookup context, thus must be unique.
 *            if(context.window("other")) {
 *              if(context.button("hello!", colors::red))
 *                ;
 *
 *              // states should be managed manually
 *              if(context.checkbox("somebox", &this->state_boolean))
 *                ; // true on change
 *
 *              // context provides blackboard to manage variables without declaration
 *              if(context.textbox("##textlabel", &context.ref<std::string>("some_value"))
 *                ; // true on text change
 *
 *              ...
 *            }
 *          }
 * \endcode
 */
class context
{
   public:
    void new_frame(graphic*);
    bool window(std::string_view name);
};
}  // namespace perfkit::gui::imgui