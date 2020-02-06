/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#pragma once

#include <exception>
#include <string>

namespace eosio {

/**
 *  @defgroup system System
 *  @ingroup core
 *  @brief Defines wrappers over eosio_assert
 */

/**
 *  Assert if the predicate fails and use the supplied message.
 *
 *  @ingroup system
 *
 *  Example:
 *  @code
 *  eosio::check(a == b, "a does not equal b");
 *  @endcode
 */
inline void check(bool pred, const char* msg) {
   if (!pred) {
      throw std::runtime_error(msg);
   }
}

/**
 *  Assert if the predicate fails and use the supplied message.
 *
 *  @ingroup system
 *
 *  Example:
 *  @code
 *  eosio::check(a == b, "a does not equal b");
 *  @endcode
 */
inline void check(bool pred, const std::string& msg) {
   if (!pred) {
      throw std::runtime_error(msg);
   }
}

/**
 *  Assert if the predicate fails and use the supplied message.
 *
 *  @ingroup system
 *
 *  Example:
 *  @code
 *  eosio::check(a == b, "a does not equal b");
 *  @endcode
 */
inline void check(bool pred, std::string&& msg) {
   if (!pred) {
      throw std::runtime_error(msg);
   }
}

/**
 *  Assert if the predicate fails and use a subset of the supplied message.
 *
 *  @ingroup system
 *
 *  Example:
 *  @code
 *  const char* msg = "a does not equal b b does not equal a";
 *  eosio::check(a == b, "a does not equal b", 18);
 *  @endcode
 */
inline void check(bool pred, const char* msg, size_t n) {
   if (!pred) {
      throw std::runtime_error(std::string(msg, n));
   }
}

/**
 *  Assert if the predicate fails and use a subset of the supplied message.
 *
 *  @ingroup system
 *
 *  Example:
 *  @code
 *  std::string msg = "a does not equal b b does not equal a";
 *  eosio::check(a == b, msg, 18);
 *  @endcode
 */
inline void check(bool pred, const std::string& msg, size_t n) {
   if (!pred) {
      throw std::runtime_error(msg.substr(0, n));
   }
}

struct eosio_error : std::exception {
   explicit eosio_error(uint64_t code) {}
};

/**
 *  Assert if the predicate fails and use the supplied error code.
 *
 *  @ingroup system
 *
 *  Example:
 *  @code
 *  eosio::check(a == b, 13);
 *  @endcode
 */
inline void check(bool pred, uint64_t code) {
   if (!pred) {
      throw eosio_error(code);
   }
}
} // namespace eosio
