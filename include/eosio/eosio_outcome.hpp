#pragma once

#include <outcome-basic.hpp>

namespace eosio {
namespace outcome = OUTCOME_V2_NAMESPACE;

template <typename T>
using result = outcome::basic_result<T, std::error_code, outcome::policy::all_narrow>;

[[noreturn]] inline void check(std::error_code ec) {
   check(false, ec.message());
   __builtin_unreachable();
}

template <typename T>
result<T> check(result<T> r) {
   if (!r)
      check(r.error());
   return r;
}

template <typename T>
void check_discard(result<T> r) {
   if (!r)
      check(r.error());
}

} // namespace eosio
