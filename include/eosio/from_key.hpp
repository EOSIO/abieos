#pragma once

#include <eosio/stream.hpp>

namespace eosio {

template <typename First, typename Second, typename S>
result<void> from_key(std::pair<First, Second>& obj, S& stream) {
   auto r = from_key(obj.first, stream);
   if (!r)
      return r;
   return from_key(obj.second, stream);
}

template <typename T, typename S>
result<void> from_key(T& obj, S& stream) {
   if constexpr (std::is_arithmetic_v<T>) {
      auto r = stream.read_raw(obj);
      if (!r)
         return r;
      std::reverse(reinterpret_cast<char*>(&obj), reinterpret_cast<char*>(&obj + 1));
      return outcome::success();
   } else {
      result<void> r = outcome::success();
      for_each_field((T*)nullptr, [&](auto* name, auto member_ptr) {
         if (r)
            r = from_key(member_from_void(member_ptr, &obj), stream);
      });
      return r;
   }
}

} // namespace eosio
