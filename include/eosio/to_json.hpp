#pragma once

#include <eosio/fpconv.h>
#include <eosio/stream.hpp>

namespace eosio {

inline constexpr char hex_digits[] = "0123456789ABCDEF";

// todo: use hex if content isn't valid utf-8
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
      if (begin != pos) {
         r = stream.write(begin, size_t(pos - begin));
         if (!r)
            return r.error();
         begin = pos;
      }
      if (begin != end) {
         if (*begin == '"') {
            r = stream.write("\\\"");
            if (!r)
               return r.error();
         } else if (*begin == '\\') {
            r = stream.write("\\\\");
            if (!r)
               return r.error();
         } else {
            r = stream.write("\\u00");
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
      return stream.write("true");
   else
      return stream.write("false");
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
   return stream.write(b.sv());
}

template <typename S>
result<void> fp_to_json(double value, S& stream) {
   small_buffer<std::numeric_limits<double>::digits10 + 2> b;
   int                                                     n = fpconv_dtoa(value, b.pos);
   if (n <= 0)
      return stream_error::float_error;
   b.pos += n;
   return stream.write(b);
}

// clang-format off
template <typename S> result<void> int_to_json(uint8_t value, S& stream)   { return int_to_json(value, stream); }
template <typename S> result<void> int_to_json(uint16_t value, S& stream)  { return int_to_json(value, stream); }
template <typename S> result<void> int_to_json(uint32_t value, S& stream)  { return int_to_json(value, stream); }
template <typename S> result<void> int_to_json(uint64_t value, S& stream)  { return int_to_json(value, stream); }
template <typename S> result<void> int_to_json(int8_t value, S& stream)    { return int_to_json(value, stream); }
template <typename S> result<void> int_to_json(int16_t value, S& stream)   { return int_to_json(value, stream); }
template <typename S> result<void> int_to_json(int32_t value, S& stream)   { return int_to_json(value, stream); }
template <typename S> result<void> int_to_json(int64_t value, S& stream)   { return int_to_json(value, stream); }
template <typename S> result<void> int_to_json(double value, S& stream)    { return fp_to_json(value, stream); }
template <typename S> result<void> int_to_json(float value, S& stream)     { return fp_to_json(value, stream); }
// clang-format on

template <typename T, typename S>
result<void> to_json(const std::vector<T>& obj, S& stream) {
   auto r = stream.write('[');
   if (!r)
      return r.error();
   bool first = true;
   for (auto& v : obj) {
      if (!first) {
         r = stream.write(',');
         if (!r)
            return r.error();
      }
      first = false;
      r     = to_json(v, stream);
      if (!r)
         return r.error();
   }
   r = stream.write(']');
   if (!r)
      return r.error();
   return outcome::success();
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

} // namespace eosio
