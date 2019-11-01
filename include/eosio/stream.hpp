#pragma once

#include <eosio/eosio_outcome.hpp>

namespace eosio {
enum class stream_error {
   no_error,
   overrun,
   underrun,
   float_error,
}; // stream_error
} // namespace eosio

namespace std {
template <>
struct is_error_code_enum<eosio::stream_error> : true_type {};
} // namespace std

namespace eosio {

class stream_error_category_type : public std::error_category {
 public:
   const char* name() const noexcept override final { return "ConversionError"; }

   std::string message(int c) const override final {
      switch (static_cast<stream_error>(c)) {
            // clang-format off
         case stream_error::no_error:     return "No error";
         case stream_error::overrun:      return "Stream overrun";
         case stream_error::underrun:     return "Stream underrun";
         case stream_error::float_error:  return "Float error";
            // clang-format on

         default: return "unknown";
      }
   }
}; // stream_error_category_type

inline const stream_error_category_type& stream_error_category() {
   static stream_error_category_type c;
   return c;
}

inline std::error_code make_error_code(stream_error e) { return { static_cast<int>(e), stream_error_category() }; }

template <int max_size>
struct small_buffer {
   char  data[max_size];
   char* pos{ data };

   void             reverse() { std::reverse(data, pos); }
   std::string_view sv() { return { data, pos - data }; }
};

struct fixed_buf_stream {
   char* pos;
   char* end;

   fixed_buf_stream(char* pos, size_t size) : pos{ pos }, end{ pos + size } {}

   result<void> write(char ch) {
      if (pos >= end)
         return stream_error::overrun;
      *pos++ = ch;
      return outcome::success();
   }

   result<void> write(const std::string_view& sv) {
      if (pos + sv.size() >= end)
         return stream_error::overrun;
      memcpy(pos, sv.data(), sv.size());
      pos += sv.size();
      return outcome::success();
   }
};

struct size_stream {
   size_t size = 0;

   result<void> write(char ch) {
      ++size;
      return outcome::success();
   }

   result<void> write(const std::string_view& sv) {
      size += sv.size();
      return outcome::success();
   }
};

} // namespace eosio
