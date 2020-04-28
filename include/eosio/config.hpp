#pragma once

namespace eosio {

#ifdef ABIEOS_ASSERT_ERRORS
constexpr inline static bool abieos_assert_errors_v = true;
#else
constexpr inline static bool abieos_assert_errors_v = false;
#endif

} // ns eosio
