#pragma once

#include <eosio/stream.hpp>

namespace eosio {

template <typename T, typename S>
result<void> raw_to_bin(const T& val, S& stream) {
   return stream.write((const char*)&val, sizeof(val));
}

// clang-format off
template <typename S>  result<void> to_bin(bool          val, S& stream) {return raw_to_bin(val, stream);}
template <typename S>  result<void> to_bin(uint8_t       val, S& stream) {return raw_to_bin(val, stream);}
template <typename S>  result<void> to_bin(uint16_t      val, S& stream) {return raw_to_bin(val, stream);}
template <typename S>  result<void> to_bin(uint32_t      val, S& stream) {return raw_to_bin(val, stream);}
template <typename S>  result<void> to_bin(uint64_t      val, S& stream) {return raw_to_bin(val, stream);}
template <typename S>  result<void> to_bin(int8_t        val, S& stream) {return raw_to_bin(val, stream);}
template <typename S>  result<void> to_bin(int16_t       val, S& stream) {return raw_to_bin(val, stream);}
template <typename S>  result<void> to_bin(int32_t       val, S& stream) {return raw_to_bin(val, stream);}
template <typename S>  result<void> to_bin(int64_t       val, S& stream) {return raw_to_bin(val, stream);}
template <typename S>  result<void> to_bin(float         val, S& stream) {return raw_to_bin(val, stream);}
template <typename S>  result<void> to_bin(double        val, S& stream) {return raw_to_bin(val, stream);}
// clang-format on

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
result<void> to_bin(const std::vector<T>& obj, S& stream) {
   auto r = varuint32_to_bin(obj.size(), stream);
   if (!r)
      return r.error();
   if constexpr (std::is_arithmetic_v<T>) {
      r = stream.write(obj.data(), obj.size() * sizeof(T));
      if (!r)
         return r.error();
   } else {
      for (auto& x : obj) {
         r = to_bin(x, stream);
         if (!r)
            return r.error();
      }
   }
   return outcome::success();
}

template <typename T>
result<std::vector<char>> to_bin(const T& t) {
   size_stream ss;
   auto        r = to_bin(t, ss);
   if (!r)
      return r.error();
   std::vector<char> result(ss.size, 0);
   fixed_buf_stream  fbs(result.data(), result.size());
   r = to_bin(t, fbs);
   if (!r)
      return r.error();
   if (fbs.pos == fbs.end)
      return std::move(result);
   else
      return stream_error::underrun;
}

} // namespace eosio
