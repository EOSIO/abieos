#pragma once

#ifdef EOSIO_CDT_COMPILATION
#   include <eosio/check.hpp>
#   define OUTCOME_DISABLE_EXECINFO
#else
#   include <stdexcept>
#endif

#include <outcome-basic.hpp>
#include <system_error>

namespace eosio {
namespace outcome = OUTCOME_V2_NAMESPACE;

template <typename T>
using result = outcome::basic_result<T, std::error_code, outcome::policy::all_narrow>;

#ifdef EOSIO_CDT_COMPILATION
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
