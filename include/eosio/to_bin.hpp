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
result<void> to_bin(std::string_view sv, S& stream);

template <typename S>
result<void> to_bin(const std::string& s, S& stream);

template <typename T, typename S>
result<void> to_bin(const std::vector<T>& obj, S& stream);

template <typename T, typename S>
result<void> to_bin(const std::optional<T>& obj, S& stream);

template <typename... Ts, typename S>
result<void> to_bin(const std::variant<Ts...>& obj, S& stream);

template <typename... Ts, typename S>
result<void> to_bin(const std::tuple<Ts...>& obj, S& stream);

template <typename T, typename S>
result<void> to_bin(const T& obj, S& stream);

template <typename S>
result<void> varuint32_to_bin(uint64_t val, S& stream) {
   if (val >> 32)
      return stream_error::varuint_too_big;
   do {
      uint8_t b = val & 0x7f;
      val >>= 7;
      b |= ((val > 0) << 7);
      auto r = stream.write(b);
      if (!r)
         return r.error();
   } while (val);
   return outcome::success();
}

// !!! temp
inline void push_varuint32(std::vector<char>& bin, uint32_t v) {
   vector_stream st{ bin };
   varuint32_to_bin(v, st).has_value();
}

template <typename S>
result<void> to_bin(std::string_view sv, S& stream) {
   auto r = varuint32_to_bin(sv.size(), stream);
   if (!r)
      return r.error();
   r = stream.write(sv.data(), sv.size());
   if (!r)
      return r.error();
   return outcome::success();
}

template <typename S>
result<void> to_bin(const std::string& s, S& stream) {
   return to_bin(std::string_view{ s }, stream);
}

template <typename T, typename S>
result<void> to_bin_range(const T& obj, S& stream) {
   OUTCOME_TRY(varuint32_to_bin(obj.size(), stream));
   for (auto& x : obj) { OUTCOME_TRY(to_bin(x, stream)); }
   return outcome::success();
}

template <typename T, std::size_t N, typename S>
result<void> to_bin(const T (&obj)[N], S& stream) {
   OUTCOME_TRY(varuint32_to_bin(N, stream));
   if constexpr (has_bitwise_serialization<T>()) {
      OUTCOME_TRY(stream.write(reinterpret_cast<const char*>(&obj), N * sizeof(T)));
   } else {
      for (auto& x : obj) { OUTCOME_TRY(to_bin(x, stream)); }
   }
   return outcome::success();
}

template <typename T, typename S>
result<void> to_bin(const std::vector<T>& obj, S& stream) {
   OUTCOME_TRY(varuint32_to_bin(obj.size(), stream));
   if constexpr (has_bitwise_serialization<T>()) {
      OUTCOME_TRY(stream.write(reinterpret_cast<const char*>(obj.data()), obj.size() * sizeof(T)));
   } else {
      for (auto& x : obj) { OUTCOME_TRY(to_bin(x, stream)); }
   }
   return outcome::success();
}

template <typename T, typename S>
result<void> to_bin(const std::list<T>& obj, S& stream) {
   return to_bin_range(obj, stream);
}

template <typename T, typename S>
result<void> to_bin(const std::deque<T>& obj, S& stream) {
   return to_bin_range(obj, stream);
}

template <typename T, typename S>
result<void> to_bin(const std::set<T>& obj, S& stream) {
   return to_bin_range(obj, stream);
}

template <typename T, typename U, typename S>
result<void> to_bin(const std::map<T, U>& obj, S& stream) {
   return to_bin_range(obj, stream);
}

template <typename S>
result<void> to_bin(const input_stream& obj, S& stream) {
   auto r = varuint32_to_bin(obj.end - obj.pos, stream);
   if (!r)
      return r.error();
   r = stream.write(obj.pos, obj.end - obj.pos);
   if (!r)
      return r.error();
   return outcome::success();
}

template <typename First, typename Second, typename S>
result<void> to_bin(const std::pair<First, Second>& obj, S& stream) {
   OUTCOME_TRY(to_bin(obj.first, stream));
   return to_bin(obj.second, stream);
}

template <typename T, typename S>
result<void> to_bin(const std::optional<T>& obj, S& stream) {
   auto r = to_bin(obj.has_value(), stream);
   if (!r)
      return r.error();
   if (obj)
      return to_bin(*obj, stream);
   else
      return outcome::success();
}

template <typename... Ts, typename S>
result<void> to_bin(const std::variant<Ts...>& obj, S& stream) {
   auto r = varuint32_to_bin(obj.index(), stream);
   if (!r)
      return r.error();
   return std::visit([&](auto& x) { return to_bin(x, stream); }, obj);
}

template <int i, typename T, typename S>
result<void> to_bin_tuple(const T& obj, S& stream) {
   if constexpr (i < std::tuple_size_v<T>) {
      auto r = to_bin(std::get<i>(obj), stream);
      if (!r)
         return r.error();
      return to_bin_tuple<i + 1>(obj, stream);
   }
   return outcome::success();
}

template <typename... Ts, typename S>
result<void> to_bin(const std::tuple<Ts...>& obj, S& stream) {
   return to_bin_tuple<0>(obj, stream);
}

template <typename T, std::size_t N, typename S>
result<void> to_bin(const std::array<T, N>& obj, S& stream) {
   for (const T& elem : obj) { OUTCOME_TRY(to_bin(elem, stream)); }
   return outcome::success();
}

template <typename T, typename S>
result<void> to_bin(const T& obj, S& stream) {
   if constexpr (has_bitwise_serialization<T>()) {
      return stream.write(reinterpret_cast<const char*>(&obj), sizeof(obj));
   } else {
      result<void> r = outcome::success();
      for_each_field(obj, [&](auto& member) {
         if (r)
            r = to_bin(member, stream);
      });
      return r;
   }
}

template <typename T>
result<void> convert_to_bin(const T& t, std::vector<char>& bin) {
   size_stream ss;
   auto        r = to_bin(t, ss);
   if (!r)
      return r.error();
   auto orig_size = bin.size();
   bin.resize(orig_size + ss.size);
   fixed_buf_stream fbs(bin.data() + orig_size, ss.size);
   r = to_bin(t, fbs);
   if (!r)
      return r.error();
   if (fbs.pos == fbs.end)
      return outcome::success();
   else
      return stream_error::underrun;
}

template <typename T>
result<std::vector<char>> convert_to_bin(const T& t) {
   std::vector<char> result;
   auto              r = convert_to_bin(t, result);
   if (r)
      return std::move(result);
   else
      return r.error();
}

} // namespace eosio
