#pragma once

#include <eosio/chain_conversions.hpp>
#include <eosio/operators.hpp>
#include <eosio/reflection.hpp>
#include <string>

namespace eosio {

struct name {
   enum class raw : uint64_t {};
   uint64_t value = 0;

   constexpr name() = default;
   constexpr explicit name(uint64_t value) : value{ value } {}
   constexpr explicit name(name::raw value) : value{ static_cast<uint64_t>(value) } {}
   // constexpr explicit name(const char* str) : value{ eosio::string_to_name(str) } {}
   // FIXME: Use string_to_name_strict somehow
   constexpr explicit name(std::string_view str) : value{ eosio::string_to_name(str) } {}
   // constexpr explicit name(const std::string& str) : value{ eosio::string_to_name(str) } {}
   constexpr name(const name&) = default;

   explicit constexpr operator raw() const { return static_cast<raw>(value); }
   explicit           operator std::string() const { return eosio::name_to_string(value); }
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

} // namespace eosio
