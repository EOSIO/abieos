//#pragma once
//
//#if defined(__eosio_cdt__)
//#   define OUTCOME_DISABLE_EXECINFO
//#   include <eosio/check.hpp>
//#else
//#   include <stdexcept>
//#endif
//
//#include <outcome-basic.hpp>
//#include <system_error>
//
//namespace eosio {
//namespace outcome = OUTCOME_V2_NAMESPACE;
//
//template <typename T>
//using result = outcome::basic_result<T, std::error_code, outcome::policy::all_narrow>;
//
//#if defined(__eosio_cdt__)
//[[noreturn]] inline void check(std::error_code ec) {
//   check(false, ec.message());
//   __builtin_unreachable();
//}
//#else
//[[noreturn]] inline void check(std::error_code ec) { throw std::runtime_error(ec.message()); }
//#endif
//
//template <typename T>
//result<T> check(result<T> r) {
//   if (!r)
//      check(r.error());
//   return r;
//}
//
//template <typename T>
//void check_discard(result<T> r) {
//   if (!r)
//      check(r.error());
//}
//
//namespace detail {
//   template <typename T>
//   constexpr std::true_type is_outcome_result(result<T>) { return {}; }
//   template <typename T>
//   constexpr std::false_type is_outcome_result(T) { return {}; }
//
//   template <typename T>
//   constexpr inline static bool is_outcome_result_v =
//      std::is_same_v<decltype(is_outcome_result(std::declval<T>())), std::true_type>;
//
//   template <typename T>
//   decltype(auto) outcome_value_forwarder(T&& val) {
//      if constexpr (is_outcome_result_v<T>)
//         return val.value();
//      //else
//      //   return std::forward<T>(val);
//   }
//} // namespace eosio::detail
//} // namespace eosio
//
////#ifndef ABIEOS_ASSERT_ERRORS
////#define EOSIO_TRY OUTCOME_TRY
////#else
////#define EOSIO_OUTCOME2(VAR, EXP) \
////   auto VAR = eosio::detail::outcome_value_forwarder(EXP);
////#define EOSIO_OUTCOME1(EXP)
////
////#define GET_MACRO(_1, _2, NAME, ...) NAME
////#define EOSIO_TRY(...) GET_MACRO(__VA_ARGS__, \
////      EOSIO_OUTCOME2, EOSIO_OUTCOME1)(__VA_ARGS__)
////
////#endif
//
//#define EOSIO_OUTCOME2(VAR, EXP) \
//   auto VAR = eosio::detail::outcome_value_forwarder(EXP);
//#define EOSIO_OUTCOME1(EXP)
//
//#undef OUTCOME_TRY
//#define GET_MACRO(_1, _2, NAME, ...) NAME
//#define OUTCOME_TRY(...) GET_MACRO(__VA_ARGS__, \
//      EOSIO_OUTCOME2, EOSIO_OUTCOME1)(__VA_ARGS__)
