#pragma once

#include <eosio/eosio_outcome.hpp>
#include <eosio/shared_memory.hpp>
#include <rapidjson/reader.h>

namespace eosio {
enum class parse_json_error {
   no_error,

   expected_end,
   expected_null,
   expected_bool,
   expected_string,
   expected_start_object,
   expected_key,
   expected_end_object,
   expected_start_array,
   expected_end_array,
   expected_positive_uint,

   // These are from rapidjson:
   document_empty,
   document_root_not_singular,
   value_invalid,
   object_miss_name,
   object_miss_colon,
   object_miss_comma_or_curly_bracket,
   array_miss_comma_or_square_bracket,
   string_unicode_escape_invalid_hex,
   string_unicode_surrogate_invalid,
   string_escape_invalid,
   string_miss_quotation_mark,
   string_invalid_encoding,
   number_too_big,
   number_miss_fraction,
   number_miss_exponent,
   terminated,
   unspecific_syntax_error,
}; // parse_json_error
} // namespace eosio

namespace std {
template <>
struct is_error_code_enum<eosio::parse_json_error> : true_type {};
} // namespace std

namespace eosio {

class parse_json_error_category_type : public std::error_category {
 public:
   const char* name() const noexcept override final { return "ConversionError"; }

   std::string message(int c) const override final {
      switch (static_cast<parse_json_error>(c)) {
            // clang-format off
               case parse_json_error::no_error:                            return "No error";

               case parse_json_error::expected_end:                        return "Expected end of json";
               case parse_json_error::expected_null:                       return "Expected null";
               case parse_json_error::expected_bool:                       return "Expected true or false";
               case parse_json_error::expected_string:                     return "Expected string";
               case parse_json_error::expected_start_object:               return "Expected {";
               case parse_json_error::expected_key:                        return "Expected key";
               case parse_json_error::expected_end_object:                 return "Expected }";
               case parse_json_error::expected_start_array:                return "Expected [";
               case parse_json_error::expected_end_array:                  return "Expected ]";
               case parse_json_error::expected_positive_uint:              return "Expected positive integer";

               case parse_json_error::document_empty:                      return "The document is empty";
               case parse_json_error::document_root_not_singular:          return "The document root must not follow by other values";
               case parse_json_error::value_invalid:                       return "Invalid value";
               case parse_json_error::object_miss_name:                    return "Missing a name for object member";
               case parse_json_error::object_miss_colon:                   return "Missing a colon after a name of object member";
               case parse_json_error::object_miss_comma_or_curly_bracket:  return "Missing a comma or '}' after an object member";
               case parse_json_error::array_miss_comma_or_square_bracket:  return "Missing a comma or ']' after an array element";
               case parse_json_error::string_unicode_escape_invalid_hex:   return "Incorrect hex digit after \\u escape in string";
               case parse_json_error::string_unicode_surrogate_invalid:    return "The surrogate pair in string is invalid";
               case parse_json_error::string_escape_invalid:               return "Invalid escape character in string";
               case parse_json_error::string_miss_quotation_mark:          return "Missing a closing quotation mark in string";
               case parse_json_error::string_invalid_encoding:             return "Invalid encoding in string";
               case parse_json_error::number_too_big:                      return "Number too big to be stored in double";
               case parse_json_error::number_miss_fraction:                return "Miss fraction part in number";
               case parse_json_error::number_miss_exponent:                return "Miss exponent in number";
               case parse_json_error::terminated:                          return "Parsing was terminated";
               case parse_json_error::unspecific_syntax_error:             return "Unspecific syntax error";
            // clang-format on

         default: return "unknown";
      }
   }
}; // parse_json_error_category_type

inline const parse_json_error_category_type& parse_json_error_category() {
   static parse_json_error_category_type c;
   return c;
}

inline std::error_code make_error_code(parse_json_error e) {
   return { static_cast<int>(e), parse_json_error_category() };
}

inline parse_json_error convert_error(rapidjson::ParseErrorCode err) {
   switch (err) {
      // clang-format off
      case rapidjson::kParseErrorNone:                            return parse_json_error::no_error;
      case rapidjson::kParseErrorDocumentEmpty:                   return parse_json_error::document_empty;
      case rapidjson::kParseErrorDocumentRootNotSingular:         return parse_json_error::document_root_not_singular;
      case rapidjson::kParseErrorValueInvalid:                    return parse_json_error::value_invalid;
      case rapidjson::kParseErrorObjectMissName:                  return parse_json_error::object_miss_name;
      case rapidjson::kParseErrorObjectMissColon:                 return parse_json_error::object_miss_colon;
      case rapidjson::kParseErrorObjectMissCommaOrCurlyBracket:   return parse_json_error::object_miss_comma_or_curly_bracket;
      case rapidjson::kParseErrorArrayMissCommaOrSquareBracket:   return parse_json_error::array_miss_comma_or_square_bracket;
      case rapidjson::kParseErrorStringUnicodeEscapeInvalidHex:   return parse_json_error::string_unicode_escape_invalid_hex;
      case rapidjson::kParseErrorStringUnicodeSurrogateInvalid:   return parse_json_error::string_unicode_surrogate_invalid;
      case rapidjson::kParseErrorStringEscapeInvalid:             return parse_json_error::string_escape_invalid;
      case rapidjson::kParseErrorStringMissQuotationMark:         return parse_json_error::string_miss_quotation_mark;
      case rapidjson::kParseErrorStringInvalidEncoding:           return parse_json_error::string_invalid_encoding;
      case rapidjson::kParseErrorNumberTooBig:                    return parse_json_error::number_too_big;
      case rapidjson::kParseErrorNumberMissFraction:              return parse_json_error::number_miss_fraction;
      case rapidjson::kParseErrorNumberMissExponent:              return parse_json_error::number_miss_exponent;
      case rapidjson::kParseErrorTermination:                     return parse_json_error::terminated;
      case rapidjson::kParseErrorUnspecificSyntaxError:           return parse_json_error::unspecific_syntax_error;
         // clang-format on

      default: return parse_json_error::unspecific_syntax_error;
   }
}

enum class json_token_type {
   type_null,
   type_bool,
   type_string,
   type_start_object,
   type_key,
   type_end_object,
   type_start_array,
   type_end_array,
};

struct json_token {
   json_token_type  type         = {};
   std::string_view key          = {};
   bool             value_bool   = {};
   std::string_view value_string = {};
};

class json_token_stream : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, json_token_stream> {
 private:
   rapidjson::Reader             reader;
   rapidjson::InsituStringStream ss;

 public:
   json_token current_token;

   json_token_stream(char* json) : ss{ json } { reader.IterativeParseInit(); }

   bool complete() { return reader.IterativeParseComplete(); }

   result<std::reference_wrapper<const json_token>> get_next_token() {
      if (reader.IterativeParseNext<rapidjson::kParseInsituFlag | rapidjson::kParseValidateEncodingFlag |
                                    rapidjson::kParseIterativeFlag | rapidjson::kParseNumbersAsStringsFlag>(ss, *this))
         return current_token;
      else
         return convert_error(reader.GetParseErrorCode());
   }

   result<void> get_end() {
      if (!complete())
         return parse_json_error::expected_end;
      return outcome::success();
   }

   result<void> get_null() {
      auto t = get_next_token();
      if (!t)
         return t.error();
      if (t.value().get().type != json_token_type::type_null)
         return parse_json_error::expected_null;
      return outcome::success();
   }

   result<bool> get_bool() {
      auto t = get_next_token();
      if (!t)
         return t.error();
      if (t.value().get().type != json_token_type::type_bool)
         return parse_json_error::expected_bool;
      return t.value().get().value_bool;
   }

   result<std::string_view> get_string() {
      auto t = get_next_token();
      if (!t)
         return t.error();
      if (t.value().get().type != json_token_type::type_string)
         return parse_json_error::expected_string;
      return t.value().get().value_string;
   }

   result<void> get_start_object() {
      auto t = get_next_token();
      if (!t)
         return t.error();
      if (t.value().get().type != json_token_type::type_start_object)
         return parse_json_error::expected_start_object;
      return outcome::success();
   }

   result<std::string_view> get_key() {
      auto t = get_next_token();
      if (!t)
         return t.error();
      if (t.value().get().type != json_token_type::type_key)
         return parse_json_error::expected_key;
      return t.value().get().value_string;
   }

   result<void> get_end_object() {
      auto t = get_next_token();
      if (!t)
         return t.error();
      if (t.value().get().type != json_token_type::type_end_object)
         return parse_json_error::expected_end_object;
      return outcome::success();
   }

   result<void> get_start_array() {
      auto t = get_next_token();
      if (!t)
         return t.error();
      if (t.value().get().type != json_token_type::type_start_array)
         return parse_json_error::expected_start_array;
      return outcome::success();
   }

   result<void> get_end_array() {
      auto t = get_next_token();
      if (!t)
         return t.error();
      if (t.value().get().type != json_token_type::type_end_array)
         return parse_json_error::expected_end_array;
      return outcome::success();
   }

   // BaseReaderHandler methods
   bool Null() {
      current_token.type = json_token_type::type_null;
      return true;
   }
   bool Bool(bool v) {
      current_token.type       = json_token_type::type_bool;
      current_token.value_bool = v;
      return true;
   }
   bool RawNumber(const char* v, rapidjson::SizeType length, bool copy) { return String(v, length, copy); }
   bool Int(int v) { return false; }
   bool Uint(unsigned v) { return false; }
   bool Int64(int64_t v) { return false; }
   bool Uint64(uint64_t v) { return false; }
   bool Double(double v) { return false; }
   bool String(const char* v, rapidjson::SizeType length, bool) {
      current_token.type         = json_token_type::type_string;
      current_token.value_string = { v, length };
      return true;
   }
   bool StartObject() {
      current_token.type = json_token_type::type_start_object;
      return true;
   }
   bool Key(const char* v, rapidjson::SizeType length, bool) {
      current_token.key  = { v, length };
      current_token.type = json_token_type::type_key;
      return true;
   }
   bool EndObject(rapidjson::SizeType) {
      current_token.type = json_token_type::type_end_object;
      return true;
   }
   bool StartArray() {
      current_token.type = json_token_type::type_start_array;
      return true;
   }
   bool EndArray(rapidjson::SizeType) {
      current_token.type = json_token_type::type_end_array;
      return true;
   }
}; // json_token_stream

/// \exclude
template <typename T>
__attribute__((noinline)) inline result<void> parse_json(T& result, json_token_stream& stream);

/// \group parse_json_explicit Parse JSON (Explicit Types)
/// Parse JSON and convert to `result`. These overloads handle specified types.
__attribute__((noinline)) inline result<void> parse_json(std::string_view& result, json_token_stream& stream) {
   auto r = stream.get_string();
   if (!r)
      return r.error();
   result = r.value();
   return outcome::success();
}

/// \group parse_json_explicit Parse JSON (Explicit Types)
/// Parse JSON and convert to `result`. These overloads handle specified types.
__attribute__((noinline)) inline result<void> parse_json(std::string& result, json_token_stream& stream) {
   auto r = stream.get_string();
   if (!r)
      return r.error();
   result = r.value();
   return outcome::success();
}

/// \group parse_json_explicit Parse JSON (Explicit Types)
__attribute__((noinline)) inline result<void> parse_json(shared_memory<std::string_view>& result,
                                                         json_token_stream&               stream) {
   return parse_json(*result, stream);
}

/// \exclude
template <typename T>
__attribute__((noinline)) inline result<void> parse_json_uint(T& result, json_token_stream& stream) {
   auto r = stream.get_string();
   if (!r)
      return r.error();
   auto pos   = r.value().data();
   auto end   = pos + r.value().size();
   bool found = false;
   result     = 0;
   while (pos != end && *pos >= '0' && *pos <= '9') {
      result = result * 10 + *pos++ - '0';
      found  = true;
   }
   if (pos != end || !found)
      return parse_json_error::expected_positive_uint;
   return outcome::success();
}

/// \group parse_json_explicit
__attribute__((noinline)) inline result<void> parse_json(uint8_t& result, json_token_stream& stream) {
   return parse_json_uint(result, stream);
}

/// \group parse_json_explicit
__attribute__((noinline)) inline result<void> parse_json(uint16_t& result, json_token_stream& stream) {
   return parse_json_uint(result, stream);
}

/// \group parse_json_explicit
__attribute__((noinline)) inline result<void> parse_json(uint32_t& result, json_token_stream& stream) {
   return parse_json_uint(result, stream);
}

/// \group parse_json_explicit
__attribute__((noinline)) inline result<void> parse_json(uint64_t& result, json_token_stream& stream) {
   return parse_json_uint(result, stream);
}

/*
/// \group parse_json_explicit
__attribute__((noinline)) inline result<void> parse_json(int32_t& result, json_parser::token_stream& stream) {
   bool in_str = false;
   if (pos != end && *pos == '"') {
      in_str = true;
      parse_json_skip_space(pos, end);
   }
   bool neg = false;
   if (pos != end && *pos == '-') {
      neg = true;
      ++pos;
   }
   bool found = false;
   result     = 0;
   while (pos != end && *pos >= '0' && *pos <= '9') {
      result = result * 10 + *pos++ - '0';
      found  = true;
   }
   check(found, "expected integer");
   parse_json_skip_space(pos, end);
   if (in_str) {
      parse_json_expect(pos, end, '"', "expected integer");
      parse_json_skip_space(pos, end);
   }
   if (neg)
      result = -result;
}

/// \group parse_json_explicit
__attribute__((noinline)) inline result<void> parse_json(bool& result, json_parser::token_stream& stream) {
   if (end - pos >= 4 && !strncmp(pos, "true", 4)) {
      pos += 4;
      result = true;
      return parse_json_skip_space(pos, end);
   }
   if (end - pos >= 5 && !strncmp(pos, "false", 5)) {
      pos += 5;
      result = false;
      return parse_json_skip_space(pos, end);
   }
   check(false, "expected boolean");
}

/// \group parse_json_explicit
template <typename T>
__attribute__((noinline)) inline result<void> parse_json(std::vector<T>& result, json_parser::token_stream& stream) {
   parse_json_expect(pos, end, '[', "expected [");
   if (pos != end && *pos != ']') {
      while (true) {
         result.emplace_back();
         parse_json(result.back(), pos, end);
         if (pos != end && *pos == ',') {
            ++pos;
            parse_json_skip_space(pos, end);
         } else
            break;
      }
   }
   parse_json_expect(pos, end, ']', "expected ]");
}

/// \exclude
template <typename F>
inline result<void> parse_json_object(json_parser::token_stream& stream, F f) {
   parse_json_expect(pos, end, '{', "expected {");
   if (pos != end && *pos != '}') {
      while (true) {
         std::string_view key;
         parse_json(key, pos, end);
         parse_json_expect(pos, end, ':', "expected :");
         f(key);
         if (pos != end && *pos == ',') {
            ++pos;
            parse_json_skip_space(pos, end);
         } else
            break;
      }
   }
   parse_json_expect(pos, end, '}', "expected }");
}

/// \output_section Parse JSON (Reflected Objects)
/// Parse JSON and convert to `result`. This overload works with
/// [reflected objects](standardese://reflection/).
template <typename T>
__attribute__((noinline)) inline result<void> parse_json(T& result, json_parser::token_stream& stream) {
   parse_json_object(pos, end, [&](std::string_view key) {
      bool found = false;
      for_each_member((T*)nullptr, [&](std::string_view member_name, auto member) {
         if (key == member_name) {
            parse_json(member_from_void(member, &result), pos, end);
            found = true;
         }
      });
      if (!found)
         parse_json_skip_value(pos, end);
   });
}

/// \output_section Convenience Wrappers
/// Parse JSON and return result. This overload wraps the other `to_json` overloads.
template <typename T>
__attribute__((noinline)) inline T parse_json(const std::vector<char>& v) {
   const char* pos = v.data();
   const char* end = pos + v.size();
   parse_json_skip_space(pos, end);
   T result;
   parse_json(result, pos, end);
   parse_json_expect_end(pos, end);
   return result;
}

/// Parse JSON and return result. This overload wraps the other `to_json` overloads.
template <typename T>
__attribute__((noinline)) inline T parse_json(std::string_view s) {
   const char* pos = s.data();
   const char* end = pos + s.size();
   parse_json_skip_space(pos, end);
   T result;
   parse_json(result, pos, end);
   parse_json_expect_end(pos, end);
   return result;
}
*/

/// Parse JSON and return result. This overload wraps the other `to_json` overloads.
template <typename T>
__attribute__((noinline)) inline result<T> parse_json(json_token_stream& stream) {
   T    x;
   auto r = parse_json(x, stream);
   if (!r)
      return r.error();
   return x;
}

/*
/// \exclude
template <size_t I, tagged_variant_options Options, typename... NamedTypes>
__attribute__((noinline)) void parse_named_variant_impl(tagged_variant<Options, NamedTypes...>& v, size_t i,
                                                        const char*& pos, const char* end) {
   if constexpr (I < sizeof...(NamedTypes)) {
      if (i == I) {
         auto& q = v.value;
         auto& x = q.template emplace<I>();
         if constexpr (!is_named_empty_type_v<std::decay_t<decltype(x)>>) {
            parse_json_expect(pos, end, ',', "expected ,");
            parse_json(x, pos, end);
         }
      } else {
         return parse_named_variant_impl<I + 1>(v, i, pos, end);
      }
   } else {
      check(false, "invalid variant index");
   }
}

/// \group parse_json_explicit
template <tagged_variant_options Options, typename... NamedTypes>
__attribute__((noinline)) result<void> parse_json(tagged_variant<Options, NamedTypes...>& result, const char*& pos,
                                          const char* end) {
   parse_json_skip_space(pos, end);
   parse_json_expect(pos, end, '[', "expected array");

   eosio::name name;
   parse_json(name, pos, end);

   for (size_t i = 0; i < sizeof...(NamedTypes); ++i) {
      if (name == tagged_variant<Options, NamedTypes...>::keys[i]) {
         parse_named_variant_impl<0>(result, i, pos, end);
         parse_json_expect(pos, end, ']', "expected ]");
         return;
      }
   }
   check(false, "invalid variant index name");
}

/// \output_section JSON Conversion Helpers
/// Skip spaces
__attribute__((noinline)) inline result<void> parse_json_skip_space(const char*& pos, const char* end) {
   while (pos != end && (*pos == 0x09 || *pos == 0x0a || *pos == 0x0d || *pos == 0x20)) ++pos;
}

// todo
/// Skip a JSON value. Caution: only partially implemented; currently mishandles most cases.
__attribute__((noinline)) inline result<void> parse_json_skip_value(const char*& pos, const char* end) {
   while (pos != end && *pos != ',' && *pos != '}') ++pos;
}

/// Asserts `ch` is next character. `msg` is the assertion message.
__attribute__((noinline)) inline result<void> parse_json_expect(const char*& pos, const char* end, char ch, const char*
msg) { check(pos != end && *pos == ch, msg);
   ++pos;
   parse_json_skip_space(pos, end);
}

/// Asserts `pos == end`.
__attribute__((noinline)) inline result<void> parse_json_expect_end(const char*& pos, const char* end) {
   check(pos == end, "expected end of json");
}
*/

} // namespace eosio
