#pragma once

#include <deque>
#include <eosio/for_each_field.hpp>
#include <eosio/stream.hpp>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace eosio {

template <typename... Ts, typename S>
result<void> to_key(const std::tuple<Ts...>& obj, S& stream);

// to_key defines a conversion from a type to a sequence of bytes whose lexicograpical
// ordering is the same as the ordering of the original type.
//
// For any two objects of type T, a and b:
//
// - key(a) < key(b) iff a < b
// - key(a) is not a prefix of key(b)
//
// Overloads of to_key for user-defined types can be found by Koenig lookup.
//
// Abieos provides specializations of to_key for the following types
// - std::string and std::string_view
// - std::vector, std::list, std::deque
// - std::tuple
// - std::array
// - std::optional
// - std::variant
// - Arithmetic types
// - Scoped enumeration types
// - Reflected structs
// - All smart-contract related types defined by abieos
template <typename T, typename S>
result<void> to_key(const T& obj, S& stream);

template <int i, typename T, typename S>
result<void> to_key_tuple(const T& obj, S& stream) {
   if constexpr (i < std::tuple_size_v<T>) {
      OUTCOME_TRY(to_key(std::get<i>(obj), stream));
      return to_key_tuple<i + 1>(obj, stream);
   }
   return outcome::success();
}

template <typename... Ts, typename S>
result<void> to_key(const std::tuple<Ts...>& obj, S& stream) {
   return to_key_tuple<0>(obj, stream);
}

template <typename T, std::size_t N, typename S>
result<void> to_key(const std::array<T, N>& obj, S& stream) {
   for (const T& elem : obj) { OUTCOME_TRY(to_key(elem, stream)); }
   return outcome::success();
}

template <typename T, typename S>
result<void> to_key_optional(const bool* obj, S& stream) {
   if (obj == nullptr)
      return stream.write('\0');
   else if (!*obj)
      return stream.write('\1');
   else
      return stream.write('\2');
}

template <typename T>
result<char> to_key_byte(const T& obj) {
   static_assert(has_bitwise_serialization<T>() && sizeof(T) == 1, "Only works for single byte types");
   char             buf[1];
   fixed_buf_stream tmp_stream(buf, 1);
   OUTCOME_TRY(to_key(obj, tmp_stream));
   if (tmp_stream.pos == tmp_stream.end)
      return buf[0];
   else
      return stream_error::overrun; // Just to be safe.  This should never happen.
}

template <typename T, typename S>
result<void> to_key_byte_range(const T& obj, S& stream) {
   static_assert(has_bitwise_serialization<typename T::value_type>() && sizeof(typename T::value_type) == 1,
                 "Only works for containers of single byte types");
   constexpr std::ptrdiff_t max_run_length = 127;
   char                     pending_run    = 0;
   for (typename T::value_type item : obj) {
      OUTCOME_TRY(ch, to_key_byte(item));
      if (pending_run) {
         if (pending_run == max_run_length || ch != '\0') {
            OUTCOME_TRY(stream.write('\0'));
            OUTCOME_TRY(stream.write(-pending_run));
            pending_run = 0;
         } else {
            ++pending_run;
            continue;
         }
      }
      if (ch == '\0') {
         pending_run = 1;
      } else {
         OUTCOME_TRY(stream.write(ch));
      }
   }
   OUTCOME_TRY(stream.write('\0'));
   return stream.write(pending_run);
}

template <typename T, typename S>
result<void> to_key_optional(const T* obj, S& stream) {
   if constexpr (has_bitwise_serialization<T>() && sizeof(T) == 1) {
      if (obj == nullptr)
         return stream.write("\0", 2);
      else {
         char             buf[1];
         fixed_buf_stream tmp_stream(buf, 1);
         OUTCOME_TRY(to_key(*obj, tmp_stream));
         OUTCOME_TRY(stream.write(buf[0]));
         if (buf[0] == '\0')
            return stream.write('\1');
         else
            return outcome::success();
      }
   } else {
      if (obj) {
         OUTCOME_TRY(stream.write('\1'));
         return to_key(*obj, stream);
      } else {
         return stream.write('\0');
      }
   }
}

template <typename T, typename U, typename S>
result<void> to_key(const std::pair<T, U>& obj, S& stream) {
   OUTCOME_TRY(to_key(obj.first, stream));
   return to_key(obj.second, stream);
}

template <typename T, typename S>
result<void> to_key_range(const T& obj, S& stream) {
   if constexpr (has_bitwise_serialization<typename T::value_type>() && sizeof(typename T::value_type) == 1) {
      return to_key_byte_range(obj, stream);
   } else {
      for (const typename T::value_type& elem : obj) { OUTCOME_TRY(to_key_optional(&elem, stream)); }
      return to_key_optional((const typename T::value_type*)nullptr, stream);
   }
}

template <typename T, typename S>
result<void> to_key(const std::vector<T>& obj, S& stream) {
   return to_key_range(obj, stream);
}

template <typename T, typename S>
result<void> to_key(const std::list<T>& obj, S& stream) {
   return to_key_range(obj, stream);
}

template <typename T, typename S>
result<void> to_key(const std::deque<T>& obj, S& stream) {
   return to_key_range(obj, stream);
}

template <typename T, typename S>
result<void> to_key(const std::set<T>& obj, S& stream) {
   return to_key_range(obj, stream);
}

template <typename T, typename U, typename S>
result<void> to_key(const std::map<T, U>& obj, S& stream) {
   return to_key_range(obj, stream);
}

template <typename T, typename S>
result<void> to_key(const std::optional<T>& obj, S& stream) {
   return to_key_optional(obj ? &*obj : nullptr, stream);
}

// The first byte holds:
// 0-4 1's (number of additional bytes) 0 (terminator) bits
//
// The number is represented as big-endian using the low order
// bits of the first byte and all of the remaining bytes.
//
// Notes:
// - values must be encoded using the minimum number of bytes,
//   as non-canonical representations will break the sort order.
template <typename S>
result<void> to_key_varuint32(std::uint32_t obj, S& stream) {
   int num_bytes;
   if (obj < 0x80u) {
      num_bytes = 1;
   } else if (obj < 0x4000u) {
      num_bytes = 2;
   } else if (obj < 0x200000u) {
      num_bytes = 3;
   } else if (obj < 0x10000000u) {
      num_bytes = 4;
   } else {
      num_bytes = 5;
   }

   OUTCOME_TRY(stream.write(
         static_cast<char>(~(0xFFu >> (num_bytes - 1)) | (num_bytes == 5 ? 0 : (obj >> ((num_bytes - 1) * 8))))));
   for (int i = num_bytes - 2; i >= 0; --i) { OUTCOME_TRY(stream.write(static_cast<char>((obj >> i * 8) & 0xFFu))); }
   return outcome::success();
}

// for non-negative values
//  The first byte holds:
//   1 (signbit) 0-4 1's (number of additional bytes) 0 (terminator) bits
//  The value is represented as big endian
// for negative values
//  The first byte holds:
//   0 (signbit) 0-4 0's (number of additional bytes) 1 (terminator) bits
//   The value is adjusted to be positive based on the range that can
//   be represented with this number of bytes and then encoded as big endian.
//
// Notes:
// - negative values must sort before positive values
// - For negative value, numbers that need more bytes are smaller, hence
//   the encoding of the width must be opposite the encoding used for
//   non-negative values.
// - A 5-byte varint can represent values in $[-2^34, 2^34)$.  In this case,
//   the argument will be sign-extended.
template <typename S>
result<void> to_key_varint32(std::int32_t obj, S& stream) {
   static_assert(std::is_same_v<S, void>, "to_key for varint32 has been temporarily disabled");
   int  num_bytes;
   bool sign = (obj < 0);
   if (obj < 0x40 && obj >= -0x40) {
      num_bytes = 1;
   } else if (obj < 0x2000 && obj >= -0x2000) {
      num_bytes = 2;
   } else if (obj < 0x100000 && obj >= -0x100000) {
      num_bytes = 3;
   } else if (obj < 0x08000000 && obj >= -0x08000000) {
      num_bytes = 4;
   } else {
      num_bytes = 5;
   }

   unsigned char width_field;
   if (sign) {
      width_field = 0x80u >> num_bytes;
   } else {
      width_field = 0x80u | ~(0xFFu >> num_bytes);
   }
   auto          uobj       = static_cast<std::uint32_t>(obj);
   unsigned char value_mask = (0xFFu >> (num_bytes + 1));
   unsigned char high_byte  = (num_bytes == 5 ? (sign ? 0xFF : 0) : (uobj >> ((num_bytes - 1) * 8)));
   OUTCOME_TRY(stream.write(width_field | (high_byte & value_mask)));
   for (int i = num_bytes - 2; i >= 0; --i) { OUTCOME_TRY(stream.write(static_cast<char>((uobj >> i * 8) & 0xFFu))); }
   return outcome::success();
}

template <typename... Ts, typename S>
result<void> to_key(const std::variant<Ts...>& obj, S& stream) {
   OUTCOME_TRY(to_key_varuint32(static_cast<uint32_t>(obj.index()), stream));
   return std::visit([&](const auto& item) { return to_key(item, stream); }, obj);
}

template <typename S>
result<void> to_key(std::string_view obj, S& stream) {
   constexpr std::ptrdiff_t max_run_length = 127;
   for (auto iter = obj.begin(), end = obj.end(); iter != end; ++iter) {
      char ch = *iter;
      OUTCOME_TRY(stream.write(ch));
      if (ch == '\0') {
         auto run_end =
               std::find_if(iter + 1, iter + std::min(end - iter, max_run_length), [](char ch) { return ch != '\0'; });
         unsigned char run_length = (run_end - iter);
         if (run_end == end) {
            return stream.write(run_length);
         } else {
            OUTCOME_TRY(stream.write(-run_length));
         }
         iter = run_end - 1; // will be incremented immediately
      }
   }
   return stream.write("\0", 2);
}

template <typename S>
result<void> to_key(const std::string& obj, S& stream) {
   return to_key(std::string_view(obj), stream);
}

template <typename S>
result<void> to_key(bool obj, S& stream) {
   return stream.write(static_cast<char>(obj ? 1 : 0));
}

template <typename UInt, typename T>
UInt float_to_key(T value) {
   static_assert(sizeof(T) == sizeof(UInt), "Expected unsigned int of the same size");
   UInt result;
   std::memcpy(&result, &value, sizeof(T));
   UInt signbit = (static_cast<UInt>(1) << (std::numeric_limits<UInt>::digits - 1));
   UInt mask    = 0;
   if (result == signbit)
      result = 0;
   if (result & signbit)
      mask = ~mask;
   return result ^ (mask | signbit);
}

template <typename T, typename S>
result<void> to_key(const T& obj, S& stream) {
   if constexpr (std::is_floating_point_v<T>) {
      if constexpr (sizeof(T) == 4) {
         return to_key(float_to_key<uint32_t>(obj), stream);
      } else {
         static_assert(sizeof(T) == 8, "Unknown floating point type");
         return to_key(float_to_key<uint64_t>(obj), stream);
      }
   } else if constexpr (std::is_integral_v<T>) {
      auto v = static_cast<std::make_unsigned_t<T>>(obj);
      v -= static_cast<std::make_unsigned_t<T>>(std::numeric_limits<T>::min());
      std::reverse(reinterpret_cast<char*>(&v), reinterpret_cast<char*>(&v + 1));
      return stream.write_raw(v);
   } else if constexpr (std::is_enum_v<T>) {
      static_assert(!std::is_convertible_v<T, std::underlying_type_t<T>>, "Serializing unscoped enum");
      return to_key(static_cast<std::underlying_type_t<T>>(obj), stream);
   } else {
      result<void> r = outcome::success();
      eosio::for_each_field(obj, [&](const auto& member) {
         if (r)
            r = to_key(member, stream);
      });
      return r;
   }
}

template <typename T>
result<void> convert_to_key(const T& t, std::vector<char>& bin) {
   size_stream ss;
   auto        r = to_key(t, ss);
   if (!r)
      return r.error();
   auto orig_size = bin.size();
   bin.resize(orig_size + ss.size);
   fixed_buf_stream fbs(bin.data() + orig_size, ss.size);
   r = to_key(t, fbs);
   if (!r)
      return r.error();
   if (fbs.pos == fbs.end)
      return outcome::success();
   else
      return stream_error::underrun;
}

template <typename T>
result<std::vector<char>> convert_to_key(const T& t) {
   std::vector<char> result;
   auto              r = convert_to_key(t, result);
   if (r)
      return std::move(result);
   else
      return r.error();
}

} // namespace eosio
