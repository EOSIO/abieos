#pragma once

#include <date/date.h>
#include <eosio/stream.hpp>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace eosio {

template<typename T>
using cresult = outcome::basic_result<T, stream_error, outcome::policy::all_narrow>;

template<typename T>
constexpr T check(cresult<T> r) {
   if(!r) {
      check(std::error_code(r.error()));
   }
   return std::move(r.value());
}

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

inline uint64_t string_to_name(const std::string& str) { return string_to_name(str.data(), str.size()); }

[[nodiscard]] inline constexpr cresult<void> char_to_name_digit_strict(char c, uint64_t& result) {
   if (c >= 'a' && c <= 'z') {
      result = (c - 'a') + 6;
      return outcome::success();
   }
   if (c >= '1' && c <= '5') {
      result = (c - '1') + 1;
      return outcome::success();
   }
   if (c == '.') {
      result = 0;
      return outcome::success();
   }
   return stream_error::invalid_name_char;
}

[[nodiscard]] inline constexpr cresult<uint64_t> string_to_name_strict(std::string_view str) {
   uint64_t name       = 0;
   unsigned i = 0;
   for (; i < str.size() && i < 12; ++i) {
      uint64_t x = 0;
      // - this is not safe in const expression OUTCOME_TRY(char_to_name_digit_strict(str[i], x));
      auto r = char_to_name_digit_strict(str[i], x);
      if( !r ) return stream_error::invalid_name_char;
      name |= (x & 0x1f) << (64 - 5 * (i + 1));
   }
   if (i < str.size() && i == 12) {
      uint64_t x = 0;
      // - this is not safe in const expression OUTCOME_TRY(char_to_name_digit_strict(str[i], x));
      auto r = char_to_name_digit_strict(str[i], x);
      if( !r ) return stream_error::invalid_name_char;

      if (x != (x & 0xf))
         return stream_error::invalid_name_char13;
      name |= x;
      ++i;
   }
   if (i < str.size())
      return stream_error::name_too_long;
   return name;
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

inline std::string microseconds_to_str(uint64_t microseconds) {
   std::string result;

   auto append_uint = [&result](uint32_t value, int digits) {
      char  s[20];
      char* ch = s;
      while (digits--) {
         *ch++ = '0' + (value % 10);
         value /= 10;
      };
      std::reverse(s, ch);
      result.insert(result.end(), s, ch);
   };

   std::chrono::microseconds us{ microseconds };
   date::sys_days            sd(std::chrono::floor<date::days>(us));
   auto                      ymd = date::year_month_day{ sd };
   uint32_t                  ms  = (std::chrono::floor<std::chrono::milliseconds>(us) - sd.time_since_epoch()).count();
   us -= sd.time_since_epoch();
   append_uint((int)ymd.year(), 4);
   result.push_back('-');
   append_uint((unsigned)ymd.month(), 2);
   result.push_back('-');
   append_uint((unsigned)ymd.day(), 2);
   result.push_back('T');
   append_uint(ms / 3600000 % 60, 2);
   result.push_back(':');
   append_uint(ms / 60000 % 60, 2);
   result.push_back(':');
   append_uint(ms / 1000 % 60, 2);
   result.push_back('.');
   append_uint(ms % 1000, 3);
   return result;
}

[[nodiscard]] inline bool string_to_utc_seconds(uint32_t& result, const char*& s, const char* end, bool eat_fractional,
                                                bool require_end) {
   auto parse_uint = [&](uint32_t& result, int digits) {
      result = 0;
      while (digits--) {
         if (s != end && *s >= '0' && *s <= '9')
            result = result * 10 + *s++ - '0';
         else
            return false;
      }
      return true;
   };
   uint32_t y, m, d, h, min, sec;
   if (!parse_uint(y, 4))
      return false;
   if (s == end || *s++ != '-')
      return false;
   if (!parse_uint(m, 2))
      return false;
   if (s == end || *s++ != '-')
      return false;
   if (!parse_uint(d, 2))
      return false;
   if (s == end || *s++ != 'T')
      return false;
   if (!parse_uint(h, 2))
      return false;
   if (s == end || *s++ != ':')
      return false;
   if (!parse_uint(min, 2))
      return false;
   if (s == end || *s++ != ':')
      return false;
   if (!parse_uint(sec, 2))
      return false;
   result = date::sys_days(date::year(y) / m / d).time_since_epoch().count() * 86400 + h * 3600 + min * 60 + sec;
   if (eat_fractional && s != end && *s == '.') {
      ++s;
      while (s != end && *s >= '0' && *s <= '9') ++s;
   }
   return s == end || !require_end;
}

[[nodiscard]] inline bool string_to_utc_seconds(uint32_t& result, const char* s, const char* end) {
   return string_to_utc_seconds(result, s, end, true, true);
}

[[nodiscard]] inline bool string_to_utc_microseconds(uint64_t& result, const char*& s, const char* end,
                                                     bool require_end) {
   uint32_t sec;
   if (!string_to_utc_seconds(sec, s, end, false, false))
      return false;
   result = sec * 1000000ull;
   if (s == end)
      return true;
   if (*s != '.')
      return !require_end;
   ++s;
   uint32_t scale = 100000;
   while (scale >= 1 && s != end && *s >= '0' && *s <= '9') {
      result += (*s++ - '0') * scale;
      scale /= 10;
   }
   return s == end || !require_end;
}

[[nodiscard]] inline bool string_to_utc_microseconds(uint64_t& result, const char* s, const char* end) {
   return string_to_utc_microseconds(result, s, end, true);
}

[[nodiscard]] inline bool string_to_symbol_code(uint64_t& result, const char*& pos, const char* end, bool require_end) {
   while (pos != end && *pos == ' ') ++pos;
   result     = 0;
   uint32_t i = 0;
   while (pos != end && *pos >= 'A' && *pos <= 'Z') {
      if (i >= 7)
         return false;
      result |= uint64_t(*pos++) << (8 * i++);
   }
   return i && (pos == end || !require_end);
}

[[nodiscard]] inline bool string_to_symbol_code(uint64_t& result, const char* pos, const char* end) {
   return string_to_symbol_code(result, pos, end, true);
}

inline std::string symbol_code_to_string(uint64_t v) {
   std::string result;
   while (v > 0) {
      result += char(v & 0xFF);
      v >>= 8;
   }
   return result;
}

[[nodiscard]] inline bool string_to_symbol(uint64_t& result, uint8_t precision, const char*& pos, const char* end,
                                           bool require_end) {
   if (!eosio::string_to_symbol_code(result, pos, end, require_end))
      return false;
   result = (result << 8) | precision;
   return true;
}

[[nodiscard]] inline bool string_to_symbol(uint64_t& result, const char*& pos, const char* end, bool require_end) {
   uint8_t precision = 0;
   bool    found     = false;
   while (pos != end && *pos >= '0' && *pos <= '9') {
      precision = precision * 10 + (*pos - '0');
      found     = true;
      ++pos;
   }
   if (!found || pos == end || *pos++ != ',')
      return false;
   return string_to_symbol(result, precision, pos, end, require_end);
}

[[nodiscard]] inline bool string_to_symbol(uint64_t& result, const char* pos, const char* end) {
   return string_to_symbol(result, pos, end, true);
}

inline std::string symbol_to_string(uint64_t v) {
   return std::to_string(v & 0xff) + "," + eosio::symbol_code_to_string(v >> 8);
}

[[nodiscard]] inline bool string_to_asset(int64_t& amount, uint64_t& symbol, const char*& s, const char* end,
                                          bool expect_end) {
   // todo: check overflow
   while (s != end && *s == ' ') //
      ++s;
   uint64_t uamount   = 0;
   uint8_t  precision = 0;
   bool     negative  = false;
   if (s != end && *s == '-') {
      ++s;
      negative = true;
   }
   while (s != end && *s >= '0' && *s <= '9') //
      uamount = uamount * 10 + (*s++ - '0');
   if (s != end && *s == '.') {
      ++s;
      while (s != end && *s >= '0' && *s <= '9') {
         uamount = uamount * 10 + (*s++ - '0');
         ++precision;
      }
   }
   if (negative)
      uamount = -uamount;
   amount = uamount;
   uint64_t code;
   if (!eosio::string_to_symbol_code(code, s, end, expect_end))
      return false;
   symbol = (code << 8) | precision;
   return true;
}

[[nodiscard]] inline bool string_to_asset(int64_t& amount, uint64_t& symbol, const char* s, const char* end) {
   return string_to_asset(amount, symbol, s, end, true);
}

inline std::string asset_to_string(int64_t amount, uint64_t symbol) {
   std::string result;
   uint64_t    uamount;
   if (amount < 0)
      uamount = -amount;
   else
      uamount = amount;
   uint8_t precision = symbol;
   if (precision) {
      while (precision--) {
         result += '0' + uamount % 10;
         uamount /= 10;
      }
      result += '.';
   }
   do {
      result += '0' + uamount % 10;
      uamount /= 10;
   } while (uamount);
   if (amount < 0)
      result += '-';
   std::reverse(result.begin(), result.end());
   return result + ' ' + eosio::symbol_code_to_string(symbol >> 8);
}

} // namespace eosio
