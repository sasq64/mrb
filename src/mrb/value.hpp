#pragma once
#include "base.hpp"

namespace mrb {


template <typename... T>
bool call_proc(mrb_state* ruby, mrb_value handler, T... arg)
{
    if (mrb_nil_p(handler)) { return false; }

    mrb_funcall(
        ruby, handler, "call", sizeof...(arg), mrb::to_value(arg, ruby)...);
    if (ruby->exc != nullptr) {
        auto bt = mrb_funcall(ruby, mrb_obj_value(ruby->exc), "backtrace", 0);

        std::vector<std::string> backtrace;
        for(int i=0; i< ARY_LEN(mrb_ary_ptr(bt)); i++) {
            auto v = mrb_ary_entry(bt, i);
            auto s = mrb_funcall(ruby, v, "to_s", 0);
            std::string line(RSTRING_PTR(s), RSTRING_LEN(s));
            backtrace.emplace_back(line);
        }

        auto obj = mrb_funcall(ruby, mrb_obj_value(ruby->exc), "inspect", 0);
        std::string err(RSTRING_PTR(obj), RSTRING_LEN(obj));

        ruby->exc = nullptr;
        throw mrb_exception(err);

        //ErrorState::stack.push_back({ErrorType::Exception, backtrace, err});
        //fmt::print("Error: {}\n", err);
        return true;
    }
    return true;
}


struct Block
{
    mrb_value val;
    mrb_state* mrb;
    operator mrb_value() const { return val; } // NOLINT
};

// A 'Value' is used to retain a ruby mrb_value on the native side, and will
// prevent it from being garbage collected until it is destroyed
struct Value
{
    std::shared_ptr<void> ptr;
    mrb_state* mrb{};
    mrb_value val{};

    Value& operator=(Block const& b)
    {
        set_from_val(b.mrb, b.val);
        return *this;
    }

    Value(Block const& b) // NOLINT
    {
        set_from_val(b.mrb, b.val);
    }

    explicit operator bool() const { return ptr != nullptr; }

    operator mrb_value() const // NOLINT
    {
        return val;
    }

    template <typename... ARGS>
    void operator()(ARGS... args)
    {
        call_proc(mrb, val, args...);
    }

    Value() = default;

    void set_from_val(mrb_state* _mrb, mrb_value v)
    {
        ptr = std::shared_ptr<void>(this, [_mrb, v](void*) {
          //fmt::print("UNREG\n");
          mrb_gc_unregister(_mrb, v);
        });
        mrb = _mrb;
        val = v;
        // Stop value from being garbage collected
        //fmt::print("REG\n");
        mrb_gc_register(mrb, val);
    }
    template <typename T>
    Value(mrb_state* _mrb, T* p)
        : mrb{_mrb}, val(mrb::to_value(std::move(p), mrb))
    {
        set_from_val(_mrb, val);
    }

    Value(mrb_state* _mrb, mrb_value v) { set_from_val(_mrb, v); }

    template <typename T>
    T as()
    {
        return value_to<T>(val, mrb);
    }

    void clear()
    {
        ptr = nullptr;
        val = mrb_value{};
    }
};


}