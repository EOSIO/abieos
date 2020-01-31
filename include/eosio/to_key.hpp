#pragma once

#include <eosio/stream.hpp>

namespace eosio {

template <typename... Ts, typename S>
result<void> to_key(const std::tuple<Ts...>& obj, S& stream);

template <typename T, typename S>
result<void> to_key(const T& obj, S& stream);

template <int i, typename T, typename S>
result<void> to_key_tuple(const T& obj, S& stream) {
   if constexpr (i < std::tuple_size_v<T>) {
      auto r = to_key(std::get<i>(obj), stream);
      if (!r)
         return r.error();
      return to_key_tuple<i + 1>(obj, stream);
   }
   return outcome::success();
}

template <typename... Ts, typename S>
result<void> to_key(const std::tuple<Ts...>& obj, S& stream) {
   return to_key_tuple<0>(obj, stream);
}

template <typename T, typename S>
result<void> to_key(const T& obj, S& stream) {
   if constexpr (std::is_arithmetic_v<T>) {
      T v = obj;
      std::reverse(reinterpret_cast<char*>(&v), reinterpret_cast<char*>(&v + 1));
      return stream.write_raw(v);
   } else {
      result<void> r = outcome::success();
      for_each_field(obj, [&](const auto& member) {
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
