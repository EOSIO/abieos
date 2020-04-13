#pragma once

#include <deque>
#include <eosio/convert.hpp>
#include <eosio/for_each_field.hpp>
#include <eosio/stream.hpp>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <variant>
#include <vector>
#include <string_view>

namespace eosio {

template <typename T, typename S>
result<void> from_bin(T& obj, S& stream);

template <typename S>
result<void> varuint32_from_bin(uint32_t& dest, S& stream) {
   dest          = 0;
   int     shift = 0;
   uint8_t b     = 0;
   do {
      if (shift >= 35)
         return stream_error::invalid_varuint_encoding;
      auto r = from_bin(b, stream);
      if (!r)
         return r;
      dest |= uint32_t(b & 0x7f) << shift;
      shift += 7;
   } while (b & 0x80);
   return outcome::success();
}

template <typename S>
result<void> varuint64_from_bin(uint64_t& dest, S& stream) {
   dest          = 0;
   int     shift = 0;
   uint8_t b     = 0;
   do {
      if (shift >= 70)
         return stream_error::invalid_varuint_encoding;
      auto r = from_bin(b, stream);
      if (!r)
         return r;
      dest |= uint64_t(b & 0x7f) << shift;
      shift += 7;
   } while (b & 0x80);
   return outcome::success();
}

template <typename S>
result<void> varint32_from_bin(int32_t& result, S& stream) {
   uint32_t v;
   auto     r = varuint32_from_bin(v, stream);
   if (!r)
      return r;
   if (v & 1)
      result = ((~v) >> 1) | 0x8000'0000;
   else
      result = v >> 1;
   return outcome::success();
}

template <typename T, typename S>
result<void> from_bin_assoc(T& v, S& stream) {
   uint32_t size;
   OUTCOME_TRY(varuint32_from_bin(size, stream));
   for (size_t i = 0; i < size; ++i) {
      typename T::value_type elem;
      OUTCOME_TRY(from_bin(elem, stream));
      v.emplace(elem);
   }
   return outcome::success();
}

template <typename T, typename S>
result<void> from_bin_sequence(T& v, S& stream) {
   uint32_t size;
   OUTCOME_TRY(varuint32_from_bin(size, stream));
   for (size_t i = 0; i < size; ++i) {
      v.emplace_back();
      OUTCOME_TRY(from_bin(v.back(), stream));
   }
   return outcome::success();
}

template <typename T, std::size_t N, typename S>
result<void> from_bin(T (&v)[N], S& stream) {
   uint32_t size;
   OUTCOME_TRY(varuint32_from_bin(size, stream));
   if (size != N)
      return stream_error::array_size_mismatch;
   if constexpr (has_bitwise_serialization<T>()) {
      return stream.read(reinterpret_cast<char*>(v), size * sizeof(T));
   } else {
      for (size_t i = 0; i < size; ++i) { OUTCOME_TRY(from_bin(v[i], stream)); }
   }
   return outcome::success();
}

template <typename T, typename S>
result<void> from_bin(std::vector<T>& v, S& stream) {
   if constexpr (has_bitwise_serialization<T>()) {
      if constexpr (sizeof(size_t) >= 8) {
         uint64_t size;
         auto     r = varuint64_from_bin(size, stream);
         if (!r)
            return r;
         r = stream.check_available(size * sizeof(T));
         if (!r)
            return r;
         v.resize(size);
         return stream.read(reinterpret_cast<char*>(v.data()), size * sizeof(T));
      } else {
         uint32_t size;
         auto     r = varuint32_from_bin(size, stream);
         if (!r)
            return r;
         r = stream.check_available(size * sizeof(T));
         if (!r)
            return r;
         v.resize(size);
         return stream.read(reinterpret_cast<char*>(v.data()), size * sizeof(T));
      }
   } else {
      uint32_t size;
      auto     r = varuint32_from_bin(size, stream);
      if (!r)
         return r;
      v.resize(size);
      for (size_t i = 0; i < size; ++i) {
         r = from_bin(v[i], stream);
         if (!r)
            return r;
      }
   }
   return outcome::success();
}

template <typename T, typename S>
result<void> from_bin(std::set<T>& v, S& stream) {
   return from_bin_assoc(v, stream);
}

template <typename T, typename U, typename S>
result<void> from_bin(std::map<T, U>& v, S& stream) {
   uint32_t size;
   OUTCOME_TRY(varuint32_from_bin(size, stream));
   for (size_t i = 0; i < size; ++i) {
      std::pair<T, U> elem;
      OUTCOME_TRY(from_bin(elem, stream));
      v.emplace(elem);
   }
   return outcome::success();
}

template <typename T, typename S>
result<void> from_bin(std::deque<T>& v, S& stream) {
   return from_bin_sequence(v, stream);
}

template <typename T, typename S>
result<void> from_bin(std::list<T>& v, S& stream) {
   return from_bin_sequence(v, stream);
}

template <typename S>
result<void> from_bin(input_stream& obj, S& stream) {
   if constexpr (sizeof(size_t) >= 8) {
      uint64_t size;
      auto     r = varuint64_from_bin(size, stream);
      if (!r)
         return r;
      r = stream.check_available(size);
      if (!r)
         return r;
      r = stream.read_reuse_storage(obj.pos, size);
      if (!r)
         return r;
      obj.end = obj.pos + size;
   } else {
      uint32_t size;
      auto     r = varuint32_from_bin(size, stream);
      if (!r)
         return r;
      r = stream.check_available(size);
      if (!r)
         return r;
      r = stream.read_reuse_storage(obj.pos, size);
      if (!r)
         return r;
      obj.end = obj.pos + size;
   }
   return outcome::success();
}

template <typename First, typename Second, typename S>
result<void> from_bin(std::pair<First, Second>& obj, S& stream) {
   auto r = from_bin(obj.first, stream);
   if (!r)
      return r;
   return from_bin(obj.second, stream);
}

template <typename S>
inline result<void> from_bin(std::string& obj, S& stream) {
   uint32_t size;
   auto     r = varuint32_from_bin(size, stream);
   if (!r)
      return r;
   obj.resize(size);
   return stream.read(obj.data(), obj.size());
}

template <typename S>
inline result<void> from_bin(std::string_view& obj, S& stream) {
   uint32_t size;
   auto     r = varuint32_from_bin(size, stream);
   if (!r)
      return r;
   obj = std::string_view(stream.get_pos(),size);
   return stream.skip(size);
}

template <typename T, typename S>
result<void> from_bin(std::optional<T>& obj, S& stream) {
   bool present;
   auto r = from_bin(present, stream);
   if (!r)
      return r;
   if (!present) {
      obj.reset();
      return outcome::success();
   }
   obj.emplace();
   return from_bin(*obj, stream);
}

template <uint32_t I, typename... Ts, typename S>
result<void> variant_from_bin(std::variant<Ts...>& v, uint32_t i, S& stream) {
   if constexpr (I < std::variant_size_v<std::variant<Ts...>>) {
      if (i == I) {
         auto& x = v.template emplace<I>();
         return from_bin(x, stream);
      } else {
         return variant_from_bin<I + 1>(v, i, stream);
      }
   } else {
      return stream_error::bad_variant_index;
   }
}

template <typename... Ts, typename S>
result<void> from_bin(std::variant<Ts...>& obj, S& stream) {
   uint32_t u;
   auto     r = varuint32_from_bin(u, stream);
   if (!r)
      return r;
   return variant_from_bin<0>(obj, u, stream);
}

template <typename T, std::size_t N, typename S>
result<void> from_bin(std::array<T, N>& obj, S& stream) {
   for (T& elem : obj) { OUTCOME_TRY(from_bin(elem, stream)); }
   return outcome::success();
}

template <int N, typename T, typename S>
result<void> from_bin_tuple(T& obj, S& stream) {
   if constexpr (N < std::tuple_size_v<T>) {
      OUTCOME_TRY(from_bin(std::get<N>(obj), stream));
      return from_bin_tuple<N + 1>(obj, stream);
   } else {
      return outcome::success();
   }
}

template <typename... T, typename S>
result<void> from_bin(std::tuple<T...>& obj, S& stream) {
   return from_bin_tuple<0>(obj, stream);
}

template <typename T, typename S>
result<void> from_bin(T& obj, S& stream) {
   if constexpr (has_bitwise_serialization<T>()) {
      return stream.read(reinterpret_cast<char*>(&obj), sizeof(T));
   } else if constexpr (std::is_same_v<serialization_type<T>, void>) {
      result<void> r = outcome::success();
      for_each_field(obj, [&](auto& member) {
         if (r)
            r = from_bin(member, stream);
      });
      return r;
   } else {
      // TODO: This can operate in place for standard serializers
      decltype(serialize_as(obj)) temp;
      OUTCOME_TRY(from_bin(temp, stream));
      convert(temp, obj, choose_first);
      return outcome::success();
   }
}

template <typename T, typename S>
result<T> from_bin(S& stream) {
   T    obj;
   OUTCOME_TRY( from_bin(obj, stream) );
   return outcome::success(obj); 
}

template <typename T>
result<void> convert_from_bin(T& obj, const std::vector<char>& bin) {
   input_stream stream{ bin };
   return from_bin(obj, stream);
}

template <typename T>
result<T> convert_from_bin(const std::vector<char>& bin) {
   T    obj;
   OUTCOME_TRY( convert_from_bin(obj, bin) );
   return outcome::success(obj); 
}

} // namespace eosio
