#pragma once
#include "base.hpp"

namespace mrb {

struct Block
{
    mrb_value val;
    mrb_state* mrb;
    operator mrb_value() const { return val; } // NOLINT
};

struct Value
{
    static inline int dummy{};
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

    operator bool() const { return ptr != nullptr; }

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
        ptr = std::shared_ptr<void>(&dummy, [_mrb, v](void*) {
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
        ptr = std::shared_ptr<void>(&dummy, [m=mrb, v=val](void*) {
          //fmt::print("UNREG\n");
          mrb_gc_unregister(m, v);
        });
        // Stop value from being garbage collected
        //fmt::print("REG\n");
        mrb_gc_register(mrb, val);
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