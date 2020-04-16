#pragma once

#include <cmath>
#include <eosio/for_each_field.hpp>
#include <eosio/fpconv.h>
#include <eosio/stream.hpp>
#include <eosio/types.hpp>
#include <limits>
#include <optional>
#include <rapidjson/encodings.h>
#include <variant>

namespace eosio {

inline constexpr char hex_digits[] = "0123456789ABCDEF";

// Adaptors for rapidjson
struct stream_adaptor {
   stream_adaptor(const char* src, int sz) {
      int chars = std::min(sz, 4);
      memcpy(buf, src, chars);
      memset(buf + chars, 0, 4 - chars);
   }
   void Put(char ch) {}
   char Take() { return buf[idx++]; }
   char buf[4];
   int  idx = 0;
};

// Replaces any invalid utf-8 bytes with ?
template <typename S>
result<void> to_json(std::string_view sv, S& stream) {
   auto r = stream.write('"');
   if (!r)
      return r.error();
   auto begin = sv.begin();
   auto end   = sv.end();
   while (begin != end) {
      auto pos = begin;
      while (pos != end && *pos != '"' && *pos != '\\' && (unsigned char)(*pos) >= 32 && *pos != 127) ++pos;
      while (begin != pos) {
         stream_adaptor s2(begin, static_cast<std::size_t>(pos - begin));
         if (rapidjson::UTF8<>::Validate(s2, s2)) {
            OUTCOME_TRY(stream.write(begin, s2.idx));
            begin += s2.idx;
         } else {
            ++begin;
            OUTCOME_TRY(stream.write('?'));
         }
      }
      if (begin != end) {
         if (*begin == '"') {
            r = stream.write("\\\"", 2);
            if (!r)
               return r.error();
         } else if (*begin == '\\') {
            r = stream.write("\\\\", 2);
            if (!r)
               return r.error();
         } else {
            r = stream.write("\\u00", 4);
            if (!r)
               return r.error();
            r = stream.write(hex_digits[(unsigned char)(*begin) >> 4]);
            if (!r)
               return r.error();
            r = stream.write(hex_digits[(unsigned char)(*begin) & 15]);
            if (!r)
               return r.error();
         }
         ++begin;
      }
   }
   r = stream.write('"');
   if (!r)
      return r.error();
   return outcome::success();
}

template <typename S>
result<void> to_json(const std::string& s, S& stream) {
   return to_json(std::string_view{ s }, stream);
}

template <typename S>
result<void> to_json(const char* s, S& stream) {
   return to_json(std::string_view{ s }, stream);
}

/*
template <typename S>
result<void> to_json(const shared_memory<std::string_view>& s, S& stream) {
   return to_json(*s, stream);
}
*/

template <typename S>
result<void> to_json(bool value, S& stream) {
   if (value)
      return stream.write("true", 4);
   else
      return stream.write("false", 5);
}

template <typename T, typename S>
result<void> int_to_json(T value, S& stream) {
   auto                                               uvalue = std::make_unsigned_t<T>(value);
   small_buffer<std::numeric_limits<T>::digits10 + 4> b;
   bool                                               neg = value < 0;
   if (neg)
      uvalue = -uvalue;
   if (sizeof(T) > 4)
      *b.pos++ = '"';
   do {
      *b.pos++ = '0' + (uvalue % 10);
      uvalue /= 10;
   } while (uvalue);
   if (neg)
      *b.pos++ = '-';
   if (sizeof(T) > 4)
      *b.pos++ = '"';
   b.reverse();
   return stream.write(b.data, b.pos - b.data);
}

template <typename S>
result<void> fp_to_json(double value, S& stream) {
   // fpconv is not quite consistent with javascript for nans and infinities
   if (value == std::numeric_limits<double>::infinity()) {
      return stream.write("\"Infinity\"", 10);
   } else if (value == -std::numeric_limits<double>::infinity()) {
      return stream.write("\"-Infinity\"", 11);
   } else if (std::isnan(value)) {
      return stream.write("\"NaN\"", 5);
   }
   small_buffer<24> b; // fpconv_dtoa generates at most 24 characters
   int              n = fpconv_dtoa(value, b.pos);
   if (n <= 0)
      return stream_error::float_error;
   b.pos += n;
   return stream.write(b.data, b.pos - b.data);
}

// clang-format off
template <typename S> result<void> to_json(uint8_t value, S& stream)   { return int_to_json(value, stream); }
template <typename S> result<void> to_json(uint16_t value, S& stream)  { return int_to_json(value, stream); }
template <typename S> result<void> to_json(uint32_t value, S& stream)  { return int_to_json(value, stream); }
template <typename S> result<void> to_json(uint64_t value, S& stream)  { return int_to_json(value, stream); }
template <typename S> result<void> to_json(unsigned __int128 value, S& stream)   { return int_to_json(value, stream); }
template <typename S> result<void> to_json(int8_t value, S& stream)    { return int_to_json(value, stream); }
template <typename S> result<void> to_json(int16_t value, S& stream)   { return int_to_json(value, stream); }
template <typename S> result<void> to_json(int32_t value, S& stream)   { return int_to_json(value, stream); }
template <typename S> result<void> to_json(int64_t value, S& stream)   { return int_to_json(value, stream); }
template <typename S> result<void> to_json(__int128 value, S& stream)   { return int_to_json(value, stream); }
template <typename S> result<void> to_json(double value, S& stream)    { return fp_to_json(value, stream); }
template <typename S> result<void> to_json(float value, S& stream)     { return fp_to_json(value, stream); }
// clang-format on

template <typename T, typename S>
result<void> to_json(const std::vector<T>& obj, S& stream) {
   OUTCOME_TRY(stream.write('['));
   bool first = true;
   for (auto& v : obj) {
      if (first) {
         OUTCOME_TRY(increase_indent(stream));
      } else {
         OUTCOME_TRY(stream.write(','));
      }
      OUTCOME_TRY(write_newline(stream));
      first = false;
      OUTCOME_TRY(to_json(v, stream));
   }
   if (!first) {
      OUTCOME_TRY(decrease_indent(stream));
      OUTCOME_TRY(write_newline(stream));
   }
   OUTCOME_TRY(stream.write(']'));
   return outcome::success();
}

template <typename T, typename S>
result<void> to_json(const std::optional<T>& obj, S& stream) {
   if (obj) {
      return to_json(*obj, stream);
   } else {
      return stream.write("null", 4);
   }
}

template <typename... T, typename S>
result<void> to_json(const std::variant<T...>& obj, S& stream) {
   OUTCOME_TRY(stream.write('['));
   OUTCOME_TRY(increase_indent(stream));
   OUTCOME_TRY(write_newline(stream));
   OUTCOME_TRY(std::visit(
         [&](const auto& t) { return to_json(get_type_name((std::decay_t<decltype(t)>*)nullptr), stream); }, obj));
   OUTCOME_TRY(stream.write(','));
   OUTCOME_TRY(write_newline(stream));
   OUTCOME_TRY(std::visit([&](auto& x) { return to_json(x, stream); }, obj));
   OUTCOME_TRY(decrease_indent(stream));
   OUTCOME_TRY(write_newline(stream));
   return stream.write(']');
}

   template<typename>
   struct is_std_optional : std::false_type {};

   template<typename T>
   struct is_std_optional<std::optional<T>> : std::true_type {
      using value_type = T;
   };

template <typename T, typename S>
result<void> to_json(const T& t, S& stream) {
   result<void> ok    = outcome::success();
   bool         first = true;
   OUTCOME_TRY(stream.write('{'));
   eosio::for_each_field<T>([&](const char* name, auto&& member) {
      if (ok) {
          auto addfield = [&]() {
            if (first) {
               auto r = increase_indent(stream);
               if (!r) {
                  ok = r;
                  return;
               }
               first = false;
            } else {
               auto r = stream.write(',');
               if (!r) {
                  ok = r;
                  return;
               }
            }
            auto r = write_newline(stream);
            if (!r) {
               ok = r;
               return;
            }
            r = to_json(name, stream);
            if (!r) {
               ok = r;
               return;
            }
            r = write_colon(stream);
            if (!r) {
               ok = r;
               return;
            }
            r = to_json(member(&t), stream);
            if (!r) {
               ok = r;
               return;
            }
         };

         auto m = member(&t);
         using member_type = std::decay_t<decltype(m)>;
         if constexpr ( not is_std_optional<member_type>::value ) {
            addfield();
         } else {
            if( !!m ) 
               addfield();
         }
      }
   });
   OUTCOME_TRY(ok);
   if (!first) {
      OUTCOME_TRY(decrease_indent(stream));
      OUTCOME_TRY(write_newline(stream));
   }
   return stream.write('}');
}

template <typename S>
result<void> to_json_hex(const char* data, size_t size, S& stream) {
   auto r = stream.write('"');
   if (!r)
      return r.error();
   for (size_t i = 0; i < size; ++i) {
      unsigned char byte = data[i];
      r                  = stream.write(hex_digits[byte >> 4]);
      if (!r)
         return r.error();
      r = stream.write(hex_digits[byte & 15]);
      if (!r)
         return r.error();
   }
   r = stream.write('"');
   if (!r)
      return r.error();
   return outcome::success();
}

template <typename T>
result<std::string> convert_to_json(const T& t) {
   size_stream ss;
   auto        r = to_json(t, ss);
   if (!r)
      return r.error();
   std::string      result(ss.size, 0);
   fixed_buf_stream fbs(result.data(), result.size());
   r = to_json(t, fbs);
   if (!r)
      return r.error();
   if (fbs.pos == fbs.end)
      return std::move(result);
   else
      return stream_error::underrun;
}

template <typename T>
result<std::string> format_json(const T& t) {
   pretty_stream<size_stream> ss;
   auto                       r = to_json(t, ss);
   if (!r)
      return r.error();
   std::string                     result(ss.size, 0);
   pretty_stream<fixed_buf_stream> fbs(result.data(), result.size());
   r = to_json(t, fbs);
   if (!r)
      return r.error();
   if (fbs.pos == fbs.end)
      return std::move(result);
   else
      return stream_error::underrun;
}

} // namespace eosio
