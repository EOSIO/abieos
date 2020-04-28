#pragma once

//#include <eosio/eosio_outcome.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <type_traits>
#include <string.h>
#include <algorithm>
#include <system_error>
#include <string_view>

namespace eosio {
enum class stream_error {
   no_error,
   overrun,
   underrun,
   float_error,
   varuint_too_big,
   invalid_varuint_encoding,
   bad_variant_index,
   invalid_asset_format,
   array_size_mismatch,
   invalid_name_char,
   invalid_name_char13,
   name_too_long,
   json_writer_error, // !!!
}; // stream_error
} // namespace eosio

// TODO uncomment after a replacement for OUTCOME is found
//namespace std {
//template <>
//struct is_error_code_enum<eosio::stream_error> : true_type {};
//} // namespace std

namespace eosio {

class stream_error_category_type : public std::error_category {
 public:
   const char* name() const noexcept override final { return "ConversionError"; }

   std::string message(int c) const override final {
      switch (static_cast<stream_error>(c)) {
            // clang-format off
         case stream_error::no_error:                 return "No error";
         case stream_error::overrun:                  return "Stream overrun";
         case stream_error::underrun:                 return "Stream underrun";
         case stream_error::float_error:              return "Float error";
         case stream_error::varuint_too_big:          return "Varuint too big";
         case stream_error::invalid_varuint_encoding: return "Invalid varuint encoding";
         case stream_error::bad_variant_index:        return "Bad variant index";
         case stream_error::invalid_asset_format:     return "Invalid asset format";
         case stream_error::array_size_mismatch:      return "T[] size and unpacked size don't match";
         case stream_error::invalid_name_char:        return "character is not in allowed character set for names";
         case stream_error::invalid_name_char13:      return "thirteenth character in name cannot be a letter that comes after j";
         case stream_error::name_too_long:            return "string is too long to be a valid name";
         case stream_error::json_writer_error: return "Error writing json";
            // clang-format on

         default: return "unknown";
      }
   }
}; // stream_error_category_type

constexpr inline std::string_view convert_stream_error(stream_error e) {
   switch (e) {
         // clang-format off
      case stream_error::no_error:                 return "No error";
      case stream_error::overrun:                  return "Stream overrun";
      case stream_error::underrun:                 return "Stream underrun";
      case stream_error::float_error:              return "Float error";
      case stream_error::varuint_too_big:          return "Varuint too big";
      case stream_error::invalid_varuint_encoding: return "Invalid varuint encoding";
      case stream_error::bad_variant_index:        return "Bad variant index";
      case stream_error::invalid_asset_format:     return "Invalid asset format";
      case stream_error::array_size_mismatch:      return "T[] size and unpacked size don't match";
      case stream_error::invalid_name_char:        return "character is not in allowed character set for names";
      case stream_error::invalid_name_char13:      return "thirteenth character in name cannot be a letter that comes after j";
      case stream_error::name_too_long:            return "string is too long to be a valid name";
      case stream_error::json_writer_error: return "Error writing json";
         // clang-format on

      default: return "unknown";
   }
}

inline const stream_error_category_type& stream_error_category() {
   static stream_error_category_type c;
   return c;
}

inline std::error_code make_error_code(stream_error e) { return { static_cast<int>(e), stream_error_category() }; }


template<typename T>
constexpr bool has_bitwise_serialization() {
   if constexpr (std::is_arithmetic_v<T>) {
      return true;
   } else if constexpr (std::is_enum_v<T>) {
      static_assert(!std::is_convertible_v<T, std::underlying_type_t<T>>, "Serializing unscoped enum");
      return true;
   } else {
      return false;
   }
}

template <int max_size>
struct small_buffer {
   char  data[max_size];
   char* pos{ data };

   void reverse() { std::reverse(data, pos); }
};

struct vector_stream {
   std::vector<char>& data;
   vector_stream(std::vector<char>& data) : data(data) {}

   bool write(char c, std::string& err) {
      data.push_back(c);
      return true;
   }
   bool write(const void* src, std::size_t sz, std::string_view& err) {
      auto s = reinterpret_cast<const char*>(src);
      data.insert( data.end(), s, s + sz );
      return true;
   }
   template <typename T>
   bool write_raw(const T& v, std::string_view& err) {
      return write(&v, sizeof(v), err);
   }
   // TODO uncomment when OUTCOME replacement
   //result<void> write(char ch) {
   //   data.push_back(ch);
   //   return outcome::success();
   //}

   //result<void> write(const void* src, size_t size) {
   //   auto s = reinterpret_cast<const char*>(src);
   //   data.insert(data.end(), s, s + size);
   //   return outcome::success();
   //}

   //template <typename T>
   //result<void> write_raw(const T& v) {
   //   return write(&v, sizeof(v));
   //}
};

struct fixed_buf_stream {
   char* pos;
   char* end;

   fixed_buf_stream(char* pos, size_t size) : pos{ pos }, end{ pos + size } {}

   bool write(char c, std::string_view& err) {
      if ( pos >= end ) {
         err = convert_stream_error(stream_error::overrun);
         return false;
      }
      *pos++ = c;
      return true;
   }

   bool write(const void* src, std::size_t sz, std::string_view& err) {
      if (pos + sz > end) {
         err = convert_stream_error(stream_error::overrun);
         return false;
      }
      memcpy(pos, src, sz);
      pos += sz;
      return true;
   }

   template <int Size>
   bool write(const char (&src)[Size], std::string_view& err) {
      return write(src, Size, err);
   }

   template <typename T>
   bool write_raw(const T& v, std::string_view& err) {
      return write(&v, sizeof(v), err);
   }
   // TODO uncomment when OUTCOME replacement
   //result<void> write(char ch) {
   //   if (pos >= end)
   //      return stream_error::overrun;
   //   *pos++ = ch;
   //   return outcome::success();
   //}

   //result<void> write(const void* src, size_t size) {
   //   if (pos + size > end)
   //      return stream_error::overrun;
   //   memcpy(pos, src, size);
   //   pos += size;
   //   return outcome::success();
   //}

   //template <int size>
   //result<void> write(const char (&src)[size]) {
   //   return write(src, size);
   //}

   //template <typename T>
   //result<void> write_raw(const T& v) {
   //   return write(&v, sizeof(v));
   //}
};

struct size_stream {
   size_t size = 0;

   bool write(char c, std::string_view& err) {
      ++size;
      return true;
   }

   bool write(const void* src, std::size_t sz, std::string_view& err) {
      size += sz;
      return true;
   }

   template <int Size>
   bool write(const char (&src)[Size], std::string_view& err) {
      size += Size;
      return true;
   }

   template <typename T>
   bool write_raw(const T& v) {
      size += sizeof(v);
      return true;
   }
   // TODO uncomment when OUTCOME replacement
   //result<void> write(char ch) {
   //   ++size;
   //   return outcome::success();
   //}

   //result<void> write(const void* src, size_t size) {
   //   this->size += size;
   //   return outcome::success();
   //}

   //template <int size>
   //result<void> write(const char (&src)[size]) {
   //   this->size += size;
   //   return outcome::success();
   //}

   //template <typename T>
   //result<void> write_raw(const T& v) {
   //   size += sizeof(v);
   //   return outcome::success();
   //}
};

template <typename S>
bool increase_indent(S&, std::string_view& err) {
   return true;
}

template <typename S>
bool decrease_indent(S&, std::string_view& err) {
   return true;
}

template <typename S>
bool write_colon(S& s, std::string_view& err) {
   return s.write(':', err);
}

template <typename S>
bool write_newline(S&, std::string_view& err) {
   return true;
}

template <typename Base>
struct pretty_stream : Base {
   using Base::Base;
   int               indent_size = 4;
   std::vector<char> current_indent;
};

template <typename S>
bool increase_indent(pretty_stream<S>& s, std::string_view& err) {
   s.current_indent.resize(s.current_indent.size() + s.indent_size, ' ');
   return true;
}

template <typename S>
bool decrease_indent(pretty_stream<S>& s, std::string_view& err) {
   if (s.current_indent.size() < s.indent_size) {
      err = convert_stream_error(stream_error::overrun);
      return false;
   }
   s.current_indent.resize(s.current_indent.size() - s.indent_size);
   return true;
}

template <typename S>
bool write_colon(pretty_stream<S>& s, std::string_view& err) {
   return s.write(": ", 2, err);
}

template <typename S>
bool write_newline(pretty_stream<S>& s, std::string_view& err) {
   bool r = s.write('\n', err);
   if (r) {
      r = s.write(s.current_indent.data(), s.current_indent.size(), err);
   }
   return r;
}

struct input_stream {
   const char* pos;
   const char* end;

   input_stream() : pos{ nullptr }, end{ nullptr } {}
   input_stream(const char* pos, size_t size) : pos{ pos }, end{ pos + size } {}
   input_stream(const char* pos, const char* end) : pos{ pos }, end{ end } {}
   input_stream(const std::vector<char>& v) : pos{ v.data() }, end{ v.data() + v.size() } {}
   input_stream(std::string_view v) : pos{ v.data() }, end{ v.data() + v.size() } {}
   input_stream(const input_stream&) = default;

   input_stream& operator=(const input_stream&) = default;

   size_t remaining() { return end - pos; }

   bool check_available(size_t size, std::string_view& err) {
      if (size > size_t(end - pos)) {
         err = convert_stream_error(stream_error::overrun);
         return false;
      }
      return true;
   }

   auto get_pos()const { return pos; }

   bool read(void* dest, size_t size, std::string_view& err) {
      if (size > size_t(end - pos)) {
         err = convert_stream_error(stream_error::overrun);
         return false;
      }
      memcpy(dest, pos, size);
      pos += size;
      return true;
   }

   template <typename T>
   bool read_raw(T& dest, std::string_view& err) {
      return read(&dest, sizeof(dest), err);
   }

   bool skip(size_t size, std::string_view& err) {
      if (size > size_t(end - pos)) {
         err = convert_stream_error(stream_error::overrun);
         return false;
      }
      pos += size;
      return true;
   }

   bool read_reuse_storage(const char*& result, size_t size, std::string_view& err) {
      if (size > size_t(end - pos)) {
         err = convert_stream_error(stream_error::overrun);
         return false;
      }
      result = pos;
      pos += size;
      return true;
   }
};

} // namespace eosio
