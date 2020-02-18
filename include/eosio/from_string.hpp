#pragma once

#include <eosio/stream.hpp>
#include <string_view>

namespace eosio {

template <typename T, typename S>
result<T> from_string(S& stream) {
   T    obj;
   auto r = from_string(obj, stream);
   if (!r)
      return r.error();
   return obj;
}

template <typename T>
result<void> convert_from_string(T& obj, std::string_view s) {
   input_stream stream{ s };
   return from_string(obj, stream);
}

template <typename T>
result<T> convert_from_string(std::string_view s) {
   T    obj;
   auto r = convert_from_string(obj, s);
   if (!r)
      return r.error();
   return obj;
}

} // namespace eosio
