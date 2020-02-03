#pragma once

#include <eosio/chain_conversions.hpp>
#include <eosio/reflection.hpp>
#include <eosio/operators.hpp>
#include <string>

namespace eosio {

inline constexpr const char* get_type_name(bool*) { return "bool"; }
inline constexpr const char* get_type_name(int8_t*) { return "int8"; }
inline constexpr const char* get_type_name(uint8_t*) { return "uint8"; }
inline constexpr const char* get_type_name(int16_t*) { return "int16"; }
inline constexpr const char* get_type_name(uint16_t*) { return "uint16"; }
inline constexpr const char* get_type_name(int32_t*) { return "int32"; }
inline constexpr const char* get_type_name(uint32_t*) { return "uint32"; }
inline constexpr const char* get_type_name(int64_t*) { return "int64"; }
inline constexpr const char* get_type_name(uint64_t*) { return "uint64"; }
inline constexpr const char* get_type_name(float*) { return "float32"; }
inline constexpr const char* get_type_name(double*) { return "float64"; }
inline constexpr const char* get_type_name(std::string*) { return "string"; }

namespace abieos {

   struct name {
      uint64_t value = 0;

      constexpr name() = default;
      constexpr explicit name(uint64_t value) : value{ value } {}
      constexpr explicit name(const char* str) : value{ eosio::string_to_name(str) } {}
      constexpr explicit name(std::string_view str) : value{ eosio::string_to_name(str) } {}
      constexpr explicit name(const std::string& str) : value{ eosio::string_to_name(str) } {}
      constexpr name(const name&) = default;

      explicit operator std::string() const { return eosio::name_to_string(value); }
   };

   EOSIO_REFLECT(name, value);
   EOSIO_COMPARE(name);

   template <typename S>
   result<void> from_json(name& obj, S& stream) {
      OUTCOME_TRY(r, stream.get_string());
      obj = name(r);
      return eosio::outcome::success();
   }

   template <typename S>
   result<void> to_json(const name& obj, S& stream) {
      return to_json(eosio::name_to_string(obj.value), stream);
   }

} // namespace abieos

} // namespace eosio
