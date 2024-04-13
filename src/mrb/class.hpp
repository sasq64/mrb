#pragma once

#include "base.hpp"
#include "conv.hpp"
#include "get_args.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <functional>
#include <numeric>
#include <string>
#include <tuple>
#include <vector>

namespace mrb {
template <typename T, typename... ARGS>
Value new_obj(mrb_state* mrb, ARGS... args)
{
    return Value(mrb, new T(args...));
}

template <typename CLASS>
constexpr const char* class_name()
{
    return CLASS::class_name();
}

template <typename T>
RClass* make_class(mrb_state* mrb, const char* name = class_name<T>(),
                   RClass* parent = nullptr)
{
    if (parent == nullptr) {
        parent = mrb->object_class;
    }
    auto* rclass = mrb_define_class(mrb, name, parent);
    Lookup<T>::rclasses[mrb] = {
        rclass,
        { name, [](mrb_state*, void* data) { delete static_cast<T*>(data); } } };
    MRB_SET_INSTANCE_TT(rclass, MRB_TT_DATA);
    mrb_define_method(
        mrb, rclass, "initialize",
        [](mrb_state* mrb, mrb_value self) -> mrb_value {
            // fmt::print("Initialize\n");
            auto* rv = (RBasic*)self.w;
            auto* obj = new T();
            DATA_PTR(self) = (void*)obj;            // NOLINT
            DATA_TYPE(self) = &Lookup<T>::rclasses[mrb].data_type; // NOLINT
            return mrb_nil_value();
        },
        MRB_ARGS_NONE());
    return rclass;
}

template <typename T>
RClass* make_noinit_class(mrb_state* mrb, const char* name = class_name<T>(),
                          RClass* parent = nullptr)
{
    if (parent == nullptr) {
        parent = mrb->object_class;
    }
    auto* rclass = mrb_define_class(mrb, name, parent);
    Lookup<T>::rclasses[mrb] = {
        rclass,
        { name, [](mrb_state*, void* data) { delete static_cast<T*>(data); } } };
    MRB_SET_INSTANCE_TT(rclass, MRB_TT_DATA);
    return rclass;
}

template <typename T>
RClass* make_shared_ptr_class(mrb_state* mrb, const char* name = class_name<T>(),
                          RClass* parent = nullptr)
{
    if (parent == nullptr) {
        parent = mrb->object_class;
    }
    auto* rclass = mrb_define_class(mrb, name, parent);
    Lookup<T>::rclasses[mrb] = {
        rclass,
        { name, [](mrb_state*, void* data) { delete static_cast<T*>(data); } } };
    MRB_SET_INSTANCE_TT(rclass, MRB_TT_DATA);
    return rclass;
}

template <typename T>
RClass* make_module(mrb_state* mrb, const char* name = class_name<T>())
{
    auto* rclass = mrb_define_module(mrb, name);
    Lookup<T>::rclasses[mrb] = { rclass, {} };
    return rclass;
}

template <typename T, typename FN>
void set_deleter(mrb_state* mrb, FN const& f)
{
    Lookup<T>::dts[mrb].dfree =
        reinterpret_cast<void (*)(mrb_state*, void*)>(+(f));
}

template <typename T>
mrb_data_type* get_data_type(mrb_state* mrb)
{
    return &Lookup<T>::dts[mrb];
}

template <typename T>
RClass* get_class(mrb_state* mrb)
{
    return Lookup<T>::rclasses[mrb].rclass;
}

template <typename T>
mrb_value new_data_obj(mrb_state* mrb)
{
    return mrb_obj_new(mrb, Lookup<T>::rclasses[mrb].rclass, 0, nullptr);
}

template <typename CLASS, typename N>
void define_const(mrb_state* ruby, std::string const& name, N value)
{
    mrb_define_const(ruby, Lookup<CLASS>::rclasses[ruby].rclass, name.c_str(),
                     mrb::to_value(value, ruby));
}

template <typename FX, typename RET, typename... ARGS>
void add_kernel_function(mrb_state* ruby, std::string const& name, FX const& fn,
                         RET (FX::*)(ARGS...) const)
{
    static FX _fn{fn};
    mrb_define_module_function(
        ruby, ruby->kernel_module, name.c_str(),
        [](mrb_state* mrb, mrb_value) -> mrb_value {
            FX fn{_fn};
            auto args = mrb::get_args<ARGS...>(mrb);
            if constexpr (std::is_same<RET, void>()) {
                std::apply(fn, args);
                return mrb_nil_value();
            } else {
                auto res = std::apply(fn, args);
                return mrb::to_value(res, mrb);
            }
        },
        MRB_ARGS_REQ(sizeof...(ARGS)));
}

template <typename FN>
void add_kernel_function(mrb_state* ruby, std::string const& name, FN const& fn)
{
    add_kernel_function(ruby, name, fn, &FN::operator());
}

template <typename CLASS, typename FX, typename RET, typename... ARGS>
void add_class_method(mrb_state* ruby, std::string const& name, FX const& fn,
                      RET (FX::*)(ARGS...) const)
{
    static FX _fn{fn};
    mrb_define_class_method(
        ruby, Lookup<CLASS>::rclasses[ruby].rclass, name.c_str(),
        [](mrb_state* mrb, mrb_value) -> mrb_value {
            FX fn{_fn};
            auto args = mrb::get_args<ARGS...>(mrb);
            if constexpr (std::is_same<RET, void>()) {
                std::apply(fn, args);
                return mrb_nil_value();
            } else {
                auto res = std::apply(fn, args);
                return mrb::to_value(res, mrb);
            }
        },
        MRB_ARGS_REQ(sizeof...(ARGS)));
}

template <typename CLASS, typename FN>
void add_class_method(mrb_state* ruby, std::string const& name, FN const& fn)
{
    add_class_method<CLASS>(ruby, name, fn, &FN::operator());
}

template <typename CLASS, typename SELF, typename FX, typename RET,
          typename... ARGS>
void add_method(mrb_state* ruby, std::string const& name, FX const& fn,
                RET (FX::*)(SELF, ARGS...) const)
{
    static FX _fn{fn};
    auto* lu = Lookup<CLASS>::rclasses[ruby].rclass;
    if (lu == nullptr) { throw mrb_exception("Adding method to unregistered class"); }
    mrb_define_method(
        ruby, lu, name.c_str(),
        [](mrb_state* mrb, mrb_value self) -> mrb_value {
            FX fn{_fn};
            auto args = mrb::get_args<ARGS...>(mrb);
            auto&& ptr = mrb::self_to<SELF>(self);
            if constexpr (std::is_same<RET, void>()) {
                std::apply(fn, std::tuple_cat(std::make_tuple(ptr), args));
                return self;
                // return mrb_nil_value();
            } else {
                return mrb::to_value(
                    std::apply(fn, std::tuple_cat(std::make_tuple(ptr), args)),
                    mrb);
            }
        },
        MRB_ARGS_REQ(sizeof...(ARGS)));
}

template <typename CLASS, typename FN>
void add_method(mrb_state* ruby, std::string const& name, FN const& fn)
{
    add_method<CLASS>(ruby, name, fn, &FN::operator());
}

template <auto PTR, typename CLASS, typename M, typename... ARGS>
void add_method2(mrb_state* ruby, std::string const& name,
                 M (CLASS::*)(ARGS...) const)
{
    add_method<CLASS>(ruby, name, [](CLASS* c, ARGS... args) {
        // return c->*PTR(args...);
        return std::invoke(PTR, c, args...);
    });
}

template <auto PTR, typename CLASS, typename M, typename... ARGS>
void add_method2(mrb_state* ruby, std::string const& name,
                 M (CLASS::*)(ARGS...))
{
    add_method<CLASS>(ruby, name, [](CLASS* c, ARGS... args) {
        return std::invoke(PTR, c, args...);
    });
}

template <auto PTR>
void add_method(mrb_state* ruby, std::string const& name)
{
    add_method2<PTR>(ruby, name, PTR);
}

template <auto PTR, typename CLASS, typename M>
void attr_accessor(mrb_state* ruby, std::string const& name, M CLASS::*)
{
    add_method<CLASS>(ruby, name, [](CLASS* c) { return c->*PTR; });
    add_method<CLASS>(ruby, name + "=", [](CLASS* c, M v) { c->*PTR = v; });
}

template <auto PTR>
void attr_accessor(mrb_state* ruby, std::string const& name)
{
    attr_accessor<PTR>(ruby, name, PTR);
}

template <auto PTR, typename CLASS, typename M>
void attr_reader(mrb_state* ruby, std::string const& name, M CLASS::*)
{
    add_method<CLASS>(ruby, name, [](CLASS* c) { return c->*PTR; });
}

template <auto PTR>
void attr_reader(mrb_state* ruby, std::string const& name)
{
    attr_reader<PTR>(ruby, name, PTR);
}



struct mruby
{
    std::shared_ptr<mrb_state> ruby;

    mrb_state* ptr() { return ruby.get(); }

    mruby() { ruby = std::shared_ptr<mrb_state>(mrb_open()); }

    template <auto PTR>
    void add_class_method(std::string const& name)
    {
        mrb::add_class_method<PTR>(ruby.get(), name, PTR);
    }

    template <typename CLASS, typename FN>
    void add_class_method(std::string const& name, FN const& fn)
    {
        mrb::add_class_method<CLASS>(ruby.get(), name, fn, &FN::operator());
    }

    template <auto PTR>
    void add_method(std::string const& name)
    {
        mrb::add_method2<PTR>(ruby.get(), name, PTR);
    }

    template <typename CLASS, typename FN>
    void add_method(std::string const& name, FN const& fn)
    {
        mrb::add_method<CLASS>(ruby.get(), name, fn, &FN::operator());
    }

    template <typename T>
    RClass* make_class(const char* name = class_name<T>(),
                       RClass* parent = nullptr)
    {
        return mrb::make_class<T>(ruby.get(), name, parent);
    }

    template <typename T>
    RClass* make_noinit_class(const char* name = class_name<T>(),
                       RClass* parent = nullptr)
    {
        return mrb::make_class<T>(ruby.get(), name, parent);
    }

    template <auto PTR>
    void attr_reader(std::string const& name)
    {
        mrb::attr_reader<PTR>(ruby.get(), name, PTR);
    }

    template <auto PTR>
    void attr_accessor(std::string const& name)
    {
        mrb::attr_accessor<PTR>(ruby.get(), name, PTR);
    }
    template <typename T, typename FN>
    void set_deleter(mrb_state* mrb, FN const& f)
    {
        mrb::set_deleter<T, FN>(ruby.get(), f);
    }

    template <typename CLASS, typename N>
    void define_const(std::string const& name, N value)
    {
        mrb::define_const<CLASS, N>(name, value);
    }

    template <typename FN>
    void add_kernel_function(std::string const& name, FN const& fn)
    {
        mrb::add_kernel_function(ruby.get(), name, fn, &FN::operator());
    }

    void exec(std::string const& code, const char* file_name = nullptr) const
    {
        //mrb_load_string(ruby.get(), code);
        auto* ctx = mrbc_context_new(ruby.get());
        ctx->capture_errors = true;
        // Set filename and line for debug messages
        if (file_name != nullptr) {
            mrbc_filename(ruby.get(), ctx, file_name);
        }
        ctx->lineno = 1;

        // Parse and run the code in one go
        mrb_load_string_cxt(ruby.get(), code.c_str(), ctx);
        if (ruby->exc != nullptr) {
            auto obj = mrb_funcall(ruby.get(), mrb_obj_value(ruby->exc),
                                   "inspect", 0);
            auto err = value_to<std::string>(obj) + "\n";

            auto bt = mrb_funcall(ruby.get(), mrb_obj_value(ruby->exc), "backtrace", 0);
            if (!mrb_nil_p(bt)) {
                auto backtrace = value_to<std::vector<std::string>>(bt);
                ruby->exc = nullptr;
                for (auto&& line : backtrace) {
                    err += line;
                    err += "\n";
                }
            }
            throw mrb_exception(err);
            //ErrorState::stack.push_back({ErrorType::Exception, backtrace, err});
            //fmt::print("Error: {}\n", err);
        }

    }
};

} // namespace mrb
