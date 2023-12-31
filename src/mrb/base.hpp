#pragma once

extern "C"
{
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/boxing_word.h>
#include <mruby/class.h>
#include <mruby/compile.h>
#include <mruby/data.h>
#include <mruby/proc.h>
#include <mruby/string.h> // NOLINT
#include <mruby/value.h>
#include <mruby/variable.h>
}

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mrb {

inline std::vector<std::string> get_backtrace(mrb_state* ruby)
{
    auto bt = mrb_funcall(ruby, mrb_obj_value(ruby->exc), "backtrace", 0);

    std::vector<std::string> backtrace;
    for (int i = 0; i < ARY_LEN(mrb_ary_ptr(bt)); i++) {
        auto v = mrb_ary_entry(bt, i);
        auto s = mrb_funcall(ruby, v, "to_s", 0);
        std::string const line(RSTRING_PTR(s), RSTRING_LEN(s));
        backtrace.emplace_back(line);
    }
    return backtrace;
}

struct mrb_exception : public std::exception
{
    explicit mrb_exception(std::string  text) : msg(std::move(text)) {}
    std::string msg;
    [[nodiscard]] char const* what() const noexcept override { return msg.c_str(); }
};
} // namespace mrb

