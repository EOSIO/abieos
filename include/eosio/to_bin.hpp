#pragma once

#include <deque>
#include <eosio/for_each_field.hpp>
#include <eosio/stream.hpp>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>

namespace eosio {

template <typename S>
bool to_bin(std::string_view sv, S& stream, std::string_view&);

template <typename S>
bool to_bin(const std::string& s, S& stream, std::string_view&);

template <typename T, typename S>
bool to_bin(const std::vector<T>& obj, S& stream, std::string_view&);

template <typename T, typename S>
bool to_bin(const std::optional<T>& obj, S& stream, std::string_view&);

template <typename... Ts, typename S>
bool to_bin(const std::variant<Ts...>& obj, S& stream, std::string_view&);

template <typename... Ts, typename S>
bool to_bin(const std::tuple<Ts...>& obj, S& stream, std::string_view&);

template <typename T, typename S>
bool to_bin(const T& obj, S& stream, std::string_view&);

template <typename S>
bool varuint32_to_bin(uint64_t val, S& stream, std::string_view& err) {
   if (val >> 32) {
      err = convert_stream_error(stream_error::varuint_too_big);
      return false;
   }
   do {
      uint8_t b = val & 0x7f;
      val >>= 7;
      b |= ((val > 0) << 7);
      auto r = stream.write(b, err);
      if (!r)
         return false;
   } while (val);
   return true;
}

// !!! temp
// TODO fix this to properly propagate the error
inline void push_varuint32(std::vector<char>& bin, uint32_t v) {
   vector_stream st{ bin };
   std::string_view err_msg;
   varuint32_to_bin(v, st, err_msg);
}

template <typename S>
bool to_bin(std::string_view sv, S& stream, std::string_view& err) {
   auto r = varuint32_to_bin(sv.size(), stream, err);
   if (!r)
      return false;
   r = stream.write(sv.data(), sv.size(), err);
   if (!r)
      return false;
   return true;
}

template <typename S>
bool to_bin(const std::string& s, S& stream, std::string_view& err) {
   return to_bin(std::string_view{ s }, stream, err);
}

template <typename T, typename S>
bool to_bin_range(const T& obj, S& stream, std::string_view& err) {
   if (!varuint32_to_bin(obj.size(), stream, err))
      return false;
   for (auto& x : obj) {
      if (!to_bin(x, stream, err))
         return false;
   }
   return true;
}

template <typename T, std::size_t N, typename S>
bool to_bin(const T (&obj)[N], S& stream, std::string_view& err) {
   if (!varuint32_to_bin(N, stream, err))
      return false;
   if constexpr (has_bitwise_serialization<T>()) {
      if (!stream.write(reinterpret_cast<const char*>(&obj), N * sizeof(T), err))
         return false;
   } else {
      for (auto& x : obj) {
        if (!to_bin(x, stream))
           return false;
      }
   }
   return true;
}

template <typename T, typename S>
bool to_bin(const std::vector<T>& obj, S& stream, std::string_view& err) {
   if (!varuint32_to_bin(obj.size(), stream, err))
      return false;
   if constexpr (has_bitwise_serialization<T>()) {
      if (!stream.write(reinterpret_cast<const char*>(obj.data()), obj.size() * sizeof(T), err))
         return false;
   } else {
      for (auto& x : obj) {
         if (!to_bin(x, stream))
            return false;
      }
   }
   return true;
}

template <typename T, typename S>
bool to_bin(const std::list<T>& obj, S& stream, std::string_view& err) {
   return to_bin_range(obj, stream, err);
}

template <typename T, typename S>
bool to_bin(const std::deque<T>& obj, S& stream, std::string_view& err) {
   return to_bin_range(obj, stream, err);
}

template <typename T, typename S>
bool to_bin(const std::set<T>& obj, S& stream, std::string_view& err) {
   return to_bin_range(obj, stream, err);
}

template <typename T, typename U, typename S>
bool to_bin(const std::map<T, U>& obj, S& stream, std::string_view& err) {
   return to_bin_range(obj, stream, err);
}

template <typename S>
bool to_bin(const input_stream& obj, S& stream, std::string_view& err) {
   auto r = varuint32_to_bin(obj.end - obj.pos, stream, err);
   if (!r)
      return false;
   r = stream.write(obj.pos, obj.end - obj.pos, err);
   if (!r)
      return false;
   return true;
}

template <typename First, typename Second, typename S>
bool to_bin(const std::pair<First, Second>& obj, S& stream, std::string_view& err) {
   if (!to_bin(obj.first, stream, err))
      return false;
   return to_bin(obj.second, stream, err);
}

template <typename T, typename S>
bool to_bin(const std::optional<T>& obj, S& stream, std::string_view& err) {
   auto r = to_bin(obj.has_value(), stream, err);
   if (!r)
      return false;
   if (obj)
      return to_bin(*obj, stream, err);
   else
      return true;
}

template <typename... Ts, typename S>
bool to_bin(const std::variant<Ts...>& obj, S& stream, std::string_view& err) {
   auto r = varuint32_to_bin(obj.index(), stream, err);
   if (!r)
      return false;
   return std::visit([&](auto& x) { return to_bin(x, stream, err); }, obj);
}

template <int i, typename T, typename S>
bool to_bin_tuple(const T& obj, S& stream, std::string_view& err) {
   if constexpr (i < std::tuple_size_v<T>) {
      auto r = to_bin(std::get<i>(obj), stream, err);
      if (!r)
         return false;
      return to_bin_tuple<i + 1>(obj, stream, err);
   }
   return true;
}

template <typename... Ts, typename S>
bool to_bin(const std::tuple<Ts...>& obj, S& stream, std::string_view& err) {
   return to_bin_tuple<0>(obj, stream, err);
}

template <typename T, std::size_t N, typename S>
bool to_bin(const std::array<T, N>& obj, S& stream, std::string_view& err) {
   for (const T& elem : obj) {
      if (!to_bin(elem, stream))
         return false;
   }
   return true;
}

template <typename T, typename S>
bool to_bin(const T& obj, S& stream, std::string_view& err) {
   if constexpr (has_bitwise_serialization<T>()) {
      return stream.write(reinterpret_cast<const char*>(&obj), sizeof(obj), err);
   } else {
      bool r = true;
      for_each_field(obj, [&](auto& member) {
         if (r)
            r = to_bin(member, stream, err);
      });
      return r;
   }
}

template <typename T>
bool convert_to_bin(const T& t, std::vector<char>& bin, std::string_view& err) {
   size_stream ss;
   auto        r = to_bin(t, ss, err);
   if (!r)
      return false;
   auto orig_size = bin.size();
   bin.resize(orig_size + ss.size);
   fixed_buf_stream fbs(bin.data() + orig_size, ss.size);
   r = to_bin(t, fbs, err);
   if (!r)
      return false;
   if (fbs.pos == fbs.end)
      return true;
   else {
      err = convert_stream_error(stream_error::underrun);
      return false;
   }
}

template <typename T>
std::optional<std::vector<char>> convert_to_bin(const T& t, std::string_view& err) {
   std::vector<char> result;
   auto              r = convert_to_bin(t, result, err);
   if (r)
      return std::move(result);
   else
      return {};
}

} // namespace eosio
