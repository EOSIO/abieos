#pragma once

#include "from_bin.hpp"
#include "stream.hpp"
#include "types.hpp"

namespace eosio {

///
/// opaque<T> type provides a type safe alternative to input_stream to declare a field
/// to be skiped during deserialization of its containing data structure. Afterwards, 
/// the underlying value can be restored with correct type information. 
///
/// The serialization opaque<T> consists of a variable length byte count followed by the 
/// serialized bytes for a value of type T. The purpose to serialized as opaque<T> as oppose
/// to T is to allow the client to delay deserialization until the value is actually needed and
/// thus saving some CPU cycles or memory requirement. 
///
/// For example, given a foo_type,
/// 
/// <code>
///    struct foo_type {
///      uint32_t                    field1;
///      string                      field2;
///      opaque<std::vector<string>> field3;
///    };
/// </code>
///   
/// the deserialization can be implemented as follows:
/// 
/// <code>  
///   input_stream serialized_foo_stream(...);
///   foo_type foo_value; 
///   from_bin(foo_value, serialized_foo_stream);
///   if (foo_value.field1 > 1 || foo_value.field2 == "meet_precondition") {
///      auto sz = foo_value.field3.unpack_size();
///      for (size_t i = 0; i < sz; ++i) {
///         if (foo_value.field3.unpack_next() == "value_to_be_searched") {
///            do_something_when_found();
///            break;
///         }
///      }
///   }
/// </code>

template <typename T>
class opaque_base {
 protected:
   input_stream bin;

 public:
   opaque_base() = default;
   explicit opaque_base(const std::vector<char>& data) : bin(data) {}
   explicit opaque_base(input_stream strm) { eosio::from_bin(bin, strm); }

   void unpack(T& obj) { eosio::from_bin(obj, bin); }

   T unpack() {
      T obj;
      this->unpack(obj);
      return obj;
   }

   bool empty() const { return !bin.remaining(); }

   template <typename S>
   void from(S& stream) {
      eosio::from_bin(this->bin, stream);
   }
};

template <typename T>
class opaque : public opaque_base<T> {
 public:
   using opaque_base<T>::opaque_base;
};

template <typename T>
class opaque<std::vector<T>> : public opaque_base<std::vector<T>> {
 public:
   using opaque_base<std::vector<T>>::opaque_base;

   size_t unpack_size() {
      uint32_t num;
      varuint32_from_bin(num, this->bin);
      return num;
   }

   void unpack_next(T& obj) { eosio::from_bin(obj, this->bin); }

   T unpack_next() {
      T obj;
      this->unpack_next(obj);
      return obj;
   }
};

template <typename T>
constexpr auto get_opaque_type_name() {
   constexpr std::size_t  size = sizeof("opaque") + (std::string_view(get_type_name((T*)nullptr)).size() + 1);
   std::array<char, size> buffer{ 'o', 'p', 'a', 'q', 'u', 'e' };
   (variant_type_appender{ buffer.data() + 6 } + std::string_view(get_type_name((T*)nullptr)));
   buffer[buffer.size() - 1] = '\0';
   return buffer;
}

template <typename T>
constexpr auto get_opaque_vector_type_name() {
   constexpr std::size_t size =
         sizeof("opaque") + std::string_view(get_type_name((T*)nullptr)).size() + sizeof("vector") + 1;
   std::array<char, size> buffer{ 'o', 'p', 'a', 'q', 'u', 'e' };
   (variant_type_appender{ buffer.data() + 6 } + std::string_view(get_type_name((T*)nullptr)) +
    std::string_view("vector"));
   buffer[buffer.size() - 1] = '\0';
   return buffer;
}

template <typename T>
constexpr auto opaque_type_name = get_opaque_type_name<T>();

template <typename T>
constexpr auto opaque_type_name<std::vector<T>> = get_opaque_vector_type_name<T>();

template <typename T>
constexpr const char* get_type_name(opaque<T>*) {
   return opaque_type_name<T>.data();
}

template <typename T, typename S>
void from_bin(opaque<T>& obj, S& stream) {
   obj.from(stream);
}

} // namespace eosio