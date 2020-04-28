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
bool from_bin(T& obj, S& stream, std::string_view& err);

template <typename S>
bool varuint32_from_bin(uint32_t& dest, S& stream, std::string_view& err) {
   dest          = 0;
   int     shift = 0;
   uint8_t b     = 0;
   do {
      if (shift >= 35) {
         err = convert_stream_error(stream_error::invalid_varuint_encoding);
         return false;
      }
      auto r = from_bin(b, stream, err);
      if (!r)
         return r;
      dest |= uint32_t(b & 0x7f) << shift;
      shift += 7;
   } while (b & 0x80);
   return true;
}

template <typename S>
bool varuint64_from_bin(uint64_t& dest, S& stream, std::string_view& err) {
   dest          = 0;
   int     shift = 0;
   uint8_t b     = 0;
   do {
      if (shift >= 70) {
         err = convert_stream_error(stream_error::invalid_varuint_encoding);
         return false;
      }
      auto r = from_bin(b, stream, err);
      if (!r)
         return r;
      dest |= uint64_t(b & 0x7f) << shift;
      shift += 7;
   } while (b & 0x80);
   return true;
}

template <typename S>
bool varint32_from_bin(int32_t& result, S& stream, std::string_view& err) {
   uint32_t v;
   auto     r = varuint32_from_bin(v, stream, err);
   if (!r)
      return r;
   if (v & 1)
      result = ((~v) >> 1) | 0x8000'0000;
   else
      result = v >> 1;
   return true;
}

template <typename T, typename S>
bool from_bin_assoc(T& v, S& stream, std::string_view& err) {
   uint32_t size;
   auto r = varuint32_from_bin(size, stream, err);
   if (!r)
      return r;
   for (size_t i = 0; i < size; ++i) {
      typename T::value_type elem;
      r = from_bin(elem, stream, err);
      if (!r)
         return r;
      v.emplace(elem);
   }
   return true;
}

template <typename T, typename S>
bool from_bin_sequence(T& v, S& stream, std::string_view& err) {
   uint32_t size;
   auto r = varuint32_from_bin(size, stream, err);
   if (!r)
      return r;
   for (size_t i = 0; i < size; ++i) {
      v.emplace_back();
      r = from_bin(v.back(), stream, err);
      if (!r)
         return r;
   }
   return true;
}

template <typename T, std::size_t N, typename S>
bool from_bin(T (&v)[N], S& stream, std::string_view& err) {
   uint32_t size;
   auto r = varuint32_from_bin(size, stream, err);
   if (!r)
      return r;
   if (size != N) {
      err = convert_stream_error(stream_error::array_size_mismatch);
      return false;
   }
   if constexpr (has_bitwise_serialization<T>()) {
      return stream.read(reinterpret_cast<char*>(v), size * sizeof(T));
   } else {
      for (size_t i = 0; i < size; ++i) {
         r = from_bin(v[i], stream, err);
         if (!r)
            return r;
      }
   }
   return true;
}

template <typename T, typename S>
bool from_bin(std::vector<T>& v, S& stream, std::string_view& err) {
   if constexpr (has_bitwise_serialization<T>()) {
      if constexpr (sizeof(size_t) >= 8) {
         uint64_t size;
         auto     r = varuint64_from_bin(size, stream, err);
         if (!r)
            return r;
         r = stream.check_available(size * sizeof(T), err);
         if (!r)
            return r;
         v.resize(size);
         return stream.read(reinterpret_cast<char*>(v.data()), size * sizeof(T), err);
      } else {
         uint32_t size;
         auto     r = varuint32_from_bin(size, stream, err);
         if (!r)
            return r;
         r = stream.check_available(size * sizeof(T), err);
         if (!r)
            return r;
         v.resize(size);
         return stream.read(reinterpret_cast<char*>(v.data()), size * sizeof(T), err);
      }
   } else {
      uint32_t size;
      auto     r = varuint32_from_bin(size, stream, err);
      if (!r)
         return r;
      v.resize(size);
      for (size_t i = 0; i < size; ++i) {
         r = from_bin(v[i], stream, err);
         if (!r)
            return r;
      }
   }
   return true;
}

template <typename T, typename S>
bool from_bin(std::set<T>& v, S& stream, std::string_view& err) {
   return from_bin_assoc(v, stream, err);
}

template <typename T, typename U, typename S>
bool from_bin(std::map<T, U>& v, S& stream, std::string_view& err) {
   uint32_t size;
   auto r = varuint32_from_bin(size, stream, err);
   if (!r)
      return r;
   for (size_t i = 0; i < size; ++i) {
      std::pair<T, U> elem;
      r = from_bin(elem, stream, err);
      if (!r)
         return r;
      v.emplace(elem);
   }
   return true;
}

template <typename T, typename S>
bool from_bin(std::deque<T>& v, S& stream, std::string_view& err) {
   return from_bin_sequence(v, stream, err);
}

template <typename T, typename S>
bool from_bin(std::list<T>& v, S& stream, std::string_view& err) {
   return from_bin_sequence(v, stream, err);
}

template <typename S>
bool from_bin(input_stream& obj, S& stream, std::string_view& err) {
   if constexpr (sizeof(size_t) >= 8) {
      uint64_t size;
      auto     r = varuint64_from_bin(size, stream, err);
      if (!r)
         return r;
      r = stream.check_available(size, err);
      if (!r)
         return r;
      r = stream.read_reuse_storage(obj.pos, size, err);
      if (!r)
         return r;
      obj.end = obj.pos + size;
   } else {
      uint32_t size;
      auto     r = varuint32_from_bin(size, stream, err);
      if (!r)
         return r;
      r = stream.check_available(size, err);
      if (!r)
         return r;
      r = stream.read_reuse_storage(obj.pos, size, err);
      if (!r)
         return r;
      obj.end = obj.pos + size;
   }
   return true;
}

template <typename First, typename Second, typename S>
bool from_bin(std::pair<First, Second>& obj, S& stream, std::string_view& err) {
   auto r = from_bin(obj.first, stream, err);
   if (!r)
      return r;
   return from_bin(obj.second, stream, err);
}

template <typename S>
inline bool from_bin(std::string& obj, S& stream, std::string_view& err) {
   uint32_t size;
   auto     r = varuint32_from_bin(size, stream, err);
   if (!r)
      return r;
   obj.resize(size);
   return stream.read(obj.data(), obj.size(), err);
}

template <typename S>
inline bool from_bin(std::string_view& obj, S& stream, std::string_view& err) {
   uint32_t size;
   auto     r = varuint32_from_bin(size, stream, err);
   if (!r)
      return r;
   obj = std::string_view(stream.get_pos(),size);
   return stream.skip(size, err);
}

template <typename T, typename S>
bool from_bin(std::optional<T>& obj, S& stream, std::string_view& err) {
   bool present;
   auto r = from_bin(present, stream, err);
   if (!r)
      return r;
   if (!present) {
      obj.reset();
      return true;
   }
   obj.emplace();
   return from_bin(*obj, stream, err);
}

template <uint32_t I, typename... Ts, typename S>
bool variant_from_bin(std::variant<Ts...>& v, uint32_t i, S& stream, std::string_view& err) {
   if constexpr (I < std::variant_size_v<std::variant<Ts...>>) {
      if (i == I) {
         auto& x = v.template emplace<I>();
         return from_bin(x, stream, err);
      } else {
         return variant_from_bin<I + 1>(v, i, stream, err);
      }
   } else {
      err = convert_stream_error(stream_error::bad_variant_index);
      return false;
   }
}

template <typename... Ts, typename S>
bool from_bin(std::variant<Ts...>& obj, S& stream, std::string_view& err) {
   uint32_t u;
   auto     r = varuint32_from_bin(u, stream, err);
   if (!r)
      return r;
   return variant_from_bin<0>(obj, u, stream, err);
}

template <typename T, std::size_t N, typename S>
bool from_bin(std::array<T, N>& obj, S& stream, std::string_view& err) {
   bool r = true;
   for (T& elem : obj) {
      r = from_bin(elem, stream, err);
      if (!r)
         return r;
   }
   return true;
}

template <int N, typename T, typename S>
bool from_bin_tuple(T& obj, S& stream, std::string_view& err) {
   if constexpr (N < std::tuple_size_v<T>) {
      auto r = from_bin(std::get<N>(obj), stream, err);
      return from_bin_tuple<N + 1>(obj, stream, err);
   } else {
      return true;
   }
}

template <typename... T, typename S>
bool from_bin(std::tuple<T...>& obj, S& stream, std::string_view& err) {
   return from_bin_tuple<0>(obj, stream, err);
}

template <typename T, typename S>
bool from_bin(T& obj, S& stream, std::string_view& err) {
   if constexpr (has_bitwise_serialization<T>()) {
      return stream.read(reinterpret_cast<char*>(&obj), sizeof(T), err);
   } else if constexpr (std::is_same_v<serialization_type<T>, void>) {
      bool r = true;
      for_each_field(obj, [&](auto& member) {
         if (r)
            r = from_bin(member, stream, err);
      });
      return r;
   } else {
      // TODO: This can operate in place for standard serializers
      decltype(serialize_as(obj)) temp;
      auto r = from_bin(temp, stream, err);
      if (!r)
         return r;
      convert(temp, obj, choose_first);
      return true;
   }
}

//template <typename T, typename S>
//result<T> from_bin(S& stream) {
//   T    obj;
//   OUTCOME_TRY( from_bin(obj, stream) );
//   return outcome::success(obj);
//}

template <typename T>
bool convert_from_bin(T& obj, const std::vector<char>& bin, std::string_view& err) {
   input_stream stream{ bin };
   return from_bin(obj, stream, err);
}

//template <typename T>
//result<T> convert_from_bin(const std::vector<char>& bin) {
//   T    obj;
//   OUTCOME_TRY( convert_from_bin(obj, bin) );
//   return outcome::success(obj);
//}

} // namespace eosio
