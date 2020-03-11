#pragma once

#if defined(__eosio_cdt__)
#   define OUTCOME_DISABLE_EXECINFO
#   include <eosio/check.hpp>
#else
#   include <stdexcept>
#endif

#include <outcome-basic.hpp>
#include <system_error>

namespace eosio {
namespace outcome = OUTCOME_V2_NAMESPACE;

template <typename T>
using result = outcome::basic_result<T, std::error_code, outcome::policy::all_narrow>;

#if defined(__eosio_cdt__)
[[noreturn]] inline void check(std::error_code ec) {
   check(false, ec.message());
   __builtin_unreachable();
}
#else
[[noreturn]] inline void check(std::error_code ec) { throw std::runtime_error(ec.message()); }
#endif

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
