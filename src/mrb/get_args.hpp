#pragma once

#include "conv.hpp"
#include "value.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace mrb {
struct ArgN
{
    int n;
    operator int() const { return n; } // NOLINT
};

// get_spec() - generate a spec string for ruby get args

template <typename ARG>
size_t get_spec(mrb_state* mrb, std::vector<char>&, std::vector<void*>&, ARG*);

inline size_t get_spec(mrb_state*, std::vector<char>&, std::vector<void*>&,
                       mrb_state**)
{
    return 0;
}

inline size_t get_spec(mrb_state*, std::vector<char>&, std::vector<void*>&,
                       ArgN*)
{
    return 0;
}

template <typename OBJ>
inline size_t get_spec(mrb_state* mrb, std::vector<char>& target,
                       std::vector<void*>& ptrs, OBJ** p)
{
    ptrs.push_back(p);
    ptrs.push_back(&Lookup<OBJ>::dts[mrb]);
    target.push_back('d');
    return target.size();
}

inline size_t get_spec(mrb_state*, std::vector<char>& target,
                       std::vector<void*>& ptrs, mrb_sym* p)
{
    ptrs.push_back(p);
    target.push_back('n');
    return target.size();
}

inline size_t get_spec(mrb_state*, std::vector<char>& target,
                       std::vector<void*>& ptrs, mrb_int* p)
{
    ptrs.push_back(p);
    target.push_back('i');
    return target.size();
}

inline size_t get_spec(mrb_state*, std::vector<char>& target,
                       std::vector<void*>& ptrs, mrb_float* p)
{
    ptrs.push_back(p);
    target.push_back('f');
    return target.size();
}

inline size_t get_spec(mrb_state*, std::vector<char>& target,
                       std::vector<void*>& ptrs, const char** p)
{
    ptrs.push_back(p);
    target.push_back('z');
    return target.size();
}

inline size_t get_spec(mrb_state*, std::vector<char>& target,
                       std::vector<void*>& ptrs, mrb_value* p)
{
    ptrs.push_back(p);
    target.push_back('o');
    return target.size();
}

inline size_t get_spec(mrb_state*, std::vector<char>& target,
                       std::vector<void*>& ptrs, Block* p)
{
    ptrs.push_back(&p->val);
    target.push_back('&');
    return target.size();
}

inline size_t get_spec(mrb_state*, std::vector<char>& target,
                       std::vector<void*>& ptrs, mrb_bool* p)
{
    ptrs.push_back(p);
    target.push_back('b');
    return target.size();
}

template <typename VAL, size_t N>
inline size_t get_spec(mrb_state*, std::vector<char>& target,
                       std::vector<void*>& ptrs, std::array<VAL, N>* p)
{
    ptrs.push_back(p);
    target.push_back('o');
    return target.size();
}

// to_mrb and mrb_to are used to convert between C++ and ruby types as needed.
// ie std::string <-> const char*, float <=> mrb_float

template <typename T>
struct to_mrb
{
    using type = T;
};

template <>
struct to_mrb<std::string>
{
    using type = const char*;
};

template <>
struct to_mrb<std::string const&>
{
    using type = const char*;
};

template <>
struct to_mrb<float>
{
    using type = mrb_float;
};

template <>
struct to_mrb<int>
{
    using type = mrb_int;
};

template <>
struct to_mrb<unsigned int>
{
    using type = mrb_int;
};

template <>
struct to_mrb<Symbol>
{
    using type = mrb_sym;
};

template <>
struct to_mrb<Value>
{
    using type = mrb_value;
};

// template <>
// struct to_mrb<Block>
//{
//     using type = Block;
// };
//

template <typename T, size_t N>
struct to_mrb<std::array<T, N>>
{
    using type = mrb_value;
};

template <typename TARGET, typename SOURCE>
auto mrb_to(SOURCE const& s, mrb_state* mrb)
{
    if constexpr (std::is_same_v<mrb_state*, TARGET>) {
        return mrb;
    } else if constexpr (std::is_same_v<Symbol, TARGET>) {
        return Symbol{mrb_sym(s)};
    } else if constexpr (std::is_same_v<ArgN, SOURCE>) {
        return TARGET{mrb_get_argc(mrb)};
    } else if constexpr (std::is_same_v<Block, TARGET>) {
        return Block{s.val, mrb};
    } else if constexpr (std::is_same_v<Value, TARGET>) {
        return Value{mrb, s};
    } else if constexpr (std::is_same_v<mrb_value, SOURCE>) {
        return value_to<TARGET>(s, mrb);
    } else {
        return static_cast<std::remove_reference_t<TARGET>>(s);
    }
}


// template <typename Target,
//     std::enable_if_t<std::is_reference_v<Target>, bool> = true>
// Target self_to(mrb_value self)
// {
//     return *static_cast<std::remove_reference_t<Target>*>(
//         (static_cast<struct RData*>(mrb_ptr(self)))->data);
// }

template <typename Target,
          std::enable_if_t<!std::is_pointer_v<Target>, bool> = true>
Target* self_to(mrb_value self)
{
    return static_cast<Target*>(
        (static_cast<struct RData*>(mrb_ptr(self)))->data);
}

template <typename Target,
          std::enable_if_t<std::is_pointer_v<Target>, bool> = true>
Target self_to(mrb_value self)
{
    return static_cast<Target>(
        (static_cast<struct RData*>(mrb_ptr(self)))->data);
}

template <class... ARGS, size_t... A>
auto get_args(mrb_state* mrb, int* num, std::index_sequence<A...>)
{
    // A tuple to store the arguments. Types are converted to corresponding
    // mruby types
    std::tuple<typename to_mrb<ARGS>::type...> target;

    std::vector<char> v;
    std::vector<void*> arg_ptrs;

    if (num) {
        *num =  mrb_get_argc(mrb);
    }

    // Build spec string, one character per type
    ((get_spec(mrb, v, arg_ptrs, &std::get<A>(target))), ...);
    v.push_back(0);
    mrb_get_args_a(mrb, v.data(), arg_ptrs.data());
    return std::tuple{mrb_to<ARGS>(std::get<A>(target), mrb)...};
}

//! Get function args according to type list
//! ex
//!
//! auto [x,y, txt] = get_args<float, float, std::string>(mrb);
//! draw_text(x,y, txt);
//
template <class... ARGS>
auto get_args(mrb_state* mrb)
{
    return get_args<ARGS...>(mrb, nullptr,
                             std::make_index_sequence<sizeof...(ARGS)>());
}

template <class... ARGS>
auto get_args_n(mrb_state* mrb)
{
    int n = 0;
    auto res = get_args<ARGS...>(mrb, &n,
                                 std::make_index_sequence<sizeof...(ARGS)>());
    return std::tuple_cat(std::make_tuple(n), res);
}

} // namespace mrb
