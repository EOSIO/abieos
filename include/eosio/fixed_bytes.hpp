#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <eosio/from_json.hpp>
#include <eosio/operators.hpp>
#include <eosio/reflection.hpp>
#include <eosio/to_json.hpp>

namespace eosio {

template <std::size_t Size>
class fixed_bytes {
 public:
   constexpr fixed_bytes() = default;
   constexpr fixed_bytes(const std::array<std::uint8_t, Size>& arr) : value(arr) {}
   constexpr fixed_bytes(const std::uint8_t (&arr)[Size]) {
      for (std::size_t i = 0; i < Size; ++i) { value[i] = arr[i]; }
   }
   constexpr auto                 data() { return value.data(); }
   constexpr auto                 data() const { return value.data(); }
   constexpr std::size_t          size() const { return value.size(); }
   constexpr auto                 extract_as_byte_array() const { return value; }
   std::array<std::uint8_t, Size> value{};
};

template <std::size_t Size>
EOSIO_COMPARE(fixed_bytes<Size>);

using float128    = fixed_bytes<16>;
using checksum160 = fixed_bytes<20>;
using checksum256 = fixed_bytes<32>;
using checksum512 = fixed_bytes<64>;

EOSIO_REFLECT(float128, value);
EOSIO_REFLECT(checksum160, value);
EOSIO_REFLECT(checksum256, value);
EOSIO_REFLECT(checksum512, value);

template <std::size_t Size, typename S>
result<void> from_json(fixed_bytes<Size>& obj, S& stream) {
   std::vector<char> v;
   OUTCOME_TRY(eosio::from_json_hex(v, stream));
   if (v.size() != Size)
      return eosio::from_json_error::hex_string_incorrect_length;
   std::memcpy(obj.value.data(), v.data(), Size);
   return outcome::success();
}

template <std::size_t Size, typename S>
result<void> to_json(const fixed_bytes<Size>& obj, S& stream) {
   return eosio::to_json_hex((const char*)obj.value.data(), obj.value.size(), stream);
}

} // namespace eosio
