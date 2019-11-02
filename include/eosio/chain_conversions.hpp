#pragma once

#include <eosio/stream.hpp>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace eosio {

inline constexpr uint64_t char_to_name_digit(char c) {
   if (c >= 'a' && c <= 'z')
      return (c - 'a') + 6;
   if (c >= '1' && c <= '5')
      return (c - '1') + 1;
   return 0;
}

inline constexpr uint64_t string_to_name(const char* str, int size) {
   uint64_t name = 0;
   int      i    = 0;
   for (; i < size && i < 12; ++i) name |= (char_to_name_digit(str[i]) & 0x1f) << (64 - 5 * (i + 1));
   if (i < size)
      name |= char_to_name_digit(str[i]) & 0x0F;
   return name;
}

inline constexpr uint64_t string_to_name(const char* str) {
   int len = 0;
   while (str[len]) ++len;
   return string_to_name(str, len);
}

inline constexpr uint64_t string_to_name(std::string_view str) { return string_to_name(str.data(), str.size()); }

inline constexpr uint64_t string_to_name(const std::string& str) { return string_to_name(str.data(), str.size()); }

inline constexpr bool char_to_name_digit_strict(char c, uint64_t& result) {
   if (c >= 'a' && c <= 'z') {
      result = (c - 'a') + 6;
      return true;
   }
   if (c >= '1' && c <= '5') {
      result = (c - '1') + 1;
      return true;
   }
   if (c == '.') {
      result = 0;
      return true;
   }
   return false;
}

inline constexpr bool string_to_name_strict(std::string_view str, uint64_t& name) {
   name       = 0;
   unsigned i = 0;
   for (; i < str.size() && i < 12; ++i) {
      uint64_t x = 0;
      if (!char_to_name_digit_strict(str[i], x))
         return false;
      name |= (x & 0x1f) << (64 - 5 * (i + 1));
   }
   if (i < str.size() && i == 12) {
      uint64_t x = 0;
      if (!char_to_name_digit_strict(str[i], x) || x != (x & 0xf))
         return false;
      name |= x;
      ++i;
   }
   if (i < str.size())
      return false;
   return true;
}

inline std::string name_to_string(uint64_t name) {
   static const char* charmap = ".12345abcdefghijklmnopqrstuvwxyz";
   std::string        str(13, '.');

   uint64_t tmp = name;
   for (uint32_t i = 0; i <= 12; ++i) {
      char c      = charmap[tmp & (i == 0 ? 0x0f : 0x1f)];
      str[12 - i] = c;
      tmp >>= (i == 0 ? 4 : 5);
   }

   const auto last = str.find_last_not_of('.');
   return str.substr(0, last + 1);
}

} // namespace eosio
