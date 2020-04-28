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
bool to_json(std::string_view sv, S& stream, std::string_view& err) {
   auto r = stream.write('"', err);
   if (!r)
      return false;
   auto begin = sv.begin();
   auto end   = sv.end();
   while (begin != end) {
      auto pos = begin;
      while (pos != end && *pos != '"' && *pos != '\\' && (unsigned char)(*pos) >= 32 && *pos != 127) ++pos;
      while (begin != pos) {
         stream_adaptor s2(begin, static_cast<std::size_t>(pos - begin));
         if (rapidjson::UTF8<>::Validate(s2, s2)) {
            if (!stream.write(begin, s2.idx, err))
               return false;
            begin += s2.idx;
         } else {
            ++begin;
            if (!stream.write('?', err))
               return false;
         }
      }
      if (begin != end) {
         if (*begin == '"') {
            r = stream.write("\\\"", 2, err);
            if (!r)
               return false;
         } else if (*begin == '\\') {
            r = stream.write("\\\\", 2, err);
            if (!r)
               return false;
         } else {
            r = stream.write("\\u00", 4, err);
            if (!r)
               return false;
            r = stream.write(hex_digits[(unsigned char)(*begin) >> 4], err);
            if (!r)
               return false;
            r = stream.write(hex_digits[(unsigned char)(*begin) & 15], err);
            if (!r)
               return false;
         }
         ++begin;
      }
   }
   r = stream.write('"', err);
   if (!r)
      return false;
   return true;
}

template <typename S>
bool to_json(const std::string& s, S& stream, std::string_view& err) {
   return to_json(std::string_view{ s }, stream, err);
}

template <typename S>
bool to_json(const char* s, S& stream, std::string_view& err) {
   return to_json(std::string_view{ s }, stream, err);
}

/*
template <typename S>
result<void> to_json(const shared_memory<std::string_view>& s, S& stream) {
   return to_json(*s, stream);
}
*/

template <typename S>
bool to_json(bool value, S& stream, std::string_view& err) {
   if (value)
      return stream.write("true", 4, err);
   else
      return stream.write("false", 5, err);
}

template <typename T, typename S>
bool int_to_json(T value, S& stream, std::string_view& err) {
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
   return stream.write(b.data, b.pos - b.data, err);
}

template <typename S>
bool fp_to_json(double value, S& stream, std::string_view& err) {
   // fpconv is not quite consistent with javascript for nans and infinities
   if (value == std::numeric_limits<double>::infinity()) {
      return stream.write("\"Infinity\"", 10, err);
   } else if (value == -std::numeric_limits<double>::infinity()) {
      return stream.write("\"-Infinity\"", 11, err);
   } else if (std::isnan(value)) {
      return stream.write("\"NaN\"", 5, err);
   }
   small_buffer<24> b; // fpconv_dtoa generates at most 24 characters
   int              n = fpconv_dtoa(value, b.pos);
   if (n <= 0) {
      err = convert_stream_error(stream_error::float_error);
      return false;
   }
   b.pos += n;
   return stream.write(b.data, b.pos - b.data, err);
}

// clang-format off
template <typename S> bool to_json(uint8_t value, S& stream, std::string_view& err)   { return int_to_json(value, stream, err); }
template <typename S> bool to_json(uint16_t value, S& stream, std::string_view& err)  { return int_to_json(value, stream, err); }
template <typename S> bool to_json(uint32_t value, S& stream, std::string_view& err)  { return int_to_json(value, stream, err); }
template <typename S> bool to_json(uint64_t value, S& stream, std::string_view& err)  { return int_to_json(value, stream, err); }
template <typename S> bool to_json(unsigned __int128 value, S& stream, std::string_view& err)   { return int_to_json(value, stream, err); }
template <typename S> bool to_json(int8_t value, S& stream, std::string_view& err)    { return int_to_json(value, stream, err); }
template <typename S> bool to_json(int16_t value, S& stream, std::string_view& err)   { return int_to_json(value, stream, err); }
template <typename S> bool to_json(int32_t value, S& stream, std::string_view& err)   { return int_to_json(value, stream, err); }
template <typename S> bool to_json(int64_t value, S& stream, std::string_view& err)   { return int_to_json(value, stream, err); }
template <typename S> bool to_json(__int128 value, S& stream, std::string_view& err)   { return int_to_json(value, stream, err); }
template <typename S> bool to_json(double value, S& stream, std::string_view& err)    { return fp_to_json(value, stream, err); }
template <typename S> bool to_json(float value, S& stream, std::string_view& err)     { return fp_to_json(value, stream, err); }
// clang-format on

template <typename T, typename S>
bool to_json(const std::vector<T>& obj, S& stream, std::string_view& err) {
   if (!stream.write('['))
      return false;
   bool first = true;
   for (auto& v : obj) {
      if (first) {
         if (!increase_indent(stream, err))
            return false;
      } else {
         if (!stream.write(',', err))
            return false;
      }
      if (!write_newline(stream, err));
      first = false;
      if (!to_json(v, stream, err));
   }
   if (!first) {
      if (!decrease_indent(stream, err));
      if (!write_newline(stream, err));
   }
   if (!stream.write(']', err));
   return true;
}

template <typename T, typename S>
bool to_json(const std::optional<T>& obj, S& stream, std::string_view& err) {
   if (obj) {
      return to_json(*obj, stream, err);
   } else {
      return stream.write("null", 4, err);
   }
}

template <typename... T, typename S>
bool to_json(const std::variant<T...>& obj, S& stream, std::string_view& err) {
   if (!stream.write('[', err))
      return false;
   if (!increase_indent(stream, err))
      return false;
   if (!write_newline(stream, err))
      return false;
   if (!std::visit(
         [&](const auto& t) { return to_json(get_type_name((std::decay_t<decltype(t)>*)nullptr), stream, err); }, obj))
      return false;
   if (!stream.write(',', err))
      return false;
   if (!write_newline(stream, err))
      return false;
   if (!std::visit([&](auto& x) { return to_json(x, stream, err); }, obj))
      return false;
   if (!decrease_indent(stream, err))
      return false;
   if (!write_newline(stream, err))
      return false;
   return stream.write(']', err);
}

   template<typename>
   struct is_std_optional : std::false_type {};

   template<typename T>
   struct is_std_optional<std::optional<T>> : std::true_type {
      using value_type = T;
   };

template <typename T, typename S>
bool to_json(const T& t, S& stream, std::string_view& err) {
   bool ok    = true;
   bool         first = true;
   if (!stream.write('{', err));
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
               auto r = stream.write(',', err);
               if (!r) {
                  ok = r;
                  return;
               }
            }
            auto r = write_newline(stream, err);
            if (!r) {
               ok = r;
               return;
            }
            r = to_json(name, stream, err);
            if (!r) {
               ok = r;
               return;
            }
            r = write_colon(stream, err);
            if (!r) {
               ok = r;
               return;
            }
            r = to_json(member(&t), stream, err);
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
   if (!ok)
      return false;
   if (!first) {
      if (!decrease_indent(stream), err)
         return false;
      if (!write_newline(stream), err)
         return false;
   }
   return stream.write('}', err);
}

template <typename S>
bool to_json_hex(const char* data, size_t size, S& stream, std::string_view& err) {
   auto r = stream.write('"', err);
   if (!r)
      return false;
   for (size_t i = 0; i < size; ++i) {
      unsigned char byte = data[i];
      r                  = stream.write(hex_digits[byte >> 4], err);
      if (!r)
         return false;
      r = stream.write(hex_digits[byte & 15], err);
      if (!r)
         return false;
   }
   r = stream.write('"', err);
   if (!r)
      return false;
   return true;
}

template <typename T>
std::optional<std::string> convert_to_json(const T& t, std::string_view& err) {
   size_stream ss;
   auto        r = to_json(t, ss, err);
   if (!r)
      return {};
   std::string      result(ss.size, 0);
   fixed_buf_stream fbs(result.data(), result.size());
   r = to_json(t, fbs, err);
   if (!r)
      return {};
   if (fbs.pos == fbs.end)
      return std::move(result);
   else {
      err = convert_stream_error(stream_error::underrun);
      return {};
   }
}

template <typename T>
std::optional<std::string> format_json(const T& t, std::string_view& err) {
   pretty_stream<size_stream> ss;
   auto                       r = to_json(t, ss, err);
   if (!r)
      return {};
   std::string                     result(ss.size, 0);
   pretty_stream<fixed_buf_stream> fbs(result.data(), result.size());
   r = to_json(t, fbs, err);
   if (!r)
      return {};
   if (fbs.pos == fbs.end)
      return std::move(result);
   else {
      err = convert_stream_error(stream_error::underrun);
      return {};
   }
}

} // namespace eosio
