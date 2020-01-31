#pragma once

#include <eosio/reflection.hpp>
#include <utility>

#if __has_include(<boost/pfr/precise/core.hpp>)

#   include <boost/pfr/precise/core.hpp>

namespace eosio {
template <typename T, typename F>
auto for_each_field(T&& t, F&& f) -> std::enable_if_t<!reflection::has_for_each_field_v<std::decay_t<T>>> {
   return boost::pfr::for_each_field(static_cast<T&&>(t), static_cast<F&&>(f));
}
} // namespace eosio

#endif

namespace eosio {

template <typename T, typename F>
auto for_each_field(T&& t, F&& f) -> std::enable_if_t<reflection::has_for_each_field_v<std::decay_t<T>>> {
   eosio_for_each_field((std::decay_t<T>*)nullptr, [&](const char*, auto member) { f(member(&t)); });
}

template <typename T, typename F>
void for_each_field(F&& f) {
   eosio_for_each_field((T*)nullptr, f);
}

} // namespace eosio
