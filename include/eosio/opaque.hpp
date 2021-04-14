#pragma once
#include "to_bin.hpp"
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
///      if(!foo_value.field3.empty()) {
///         loop_until(foo_value.field3, [](const auto& x) {
///             if (x.size() > 1) {
///                return true;
///             }
///             do_something(x);
///             return false;
///         });
///      }
///   }
/// </code>

template <typename T>
class opaque_base {
 protected:
   input_stream bin;

   opaque_base(input_stream b) : bin(b) {}

 public:
   opaque_base() = default;
   explicit opaque_base(const std::vector<char>& data) : bin(data) {}

   /**
    * @pre !this->empty()
    */
   [[deprecated("Use unpack() free function instead.")]] void unpack(T& obj) { eosio::from_bin(obj, bin); }

   /**
    *   @pre !this->empty()
    */
   [[deprecated("Use unpack() free function instead.")]] T unpack() {
      T obj;
      this->unpack(obj);
      return obj;
   }

   bool   empty() const { return !bin.remaining(); }
   size_t num_bytes() const { return bin.remaining(); }

   template <typename S>
   void from(S& stream) {
      eosio::from_bin(this->bin, stream);
   }

   template <typename S>
   void to_bin(S& stream) const {
      eosio::to_bin(this->bin, stream);
   }

   input_stream get() const { return bin; }
};

template <typename T>
class opaque : public opaque_base<T> {
 public:
   using opaque_base<T>::opaque_base;

   template <typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>>>
   opaque(opaque<U> other) : opaque_base<T>(other.bin) {}

   template <typename U>
   friend opaque<U> as_opaque(input_stream bin);
};

template <typename T>
class opaque<std::vector<T>> : public opaque_base<std::vector<T>> {
 public:
   using opaque_base<std::vector<T>>::opaque_base;

   /** Determine the size of the vector to be unpacked.
    *
    *   @pre !this->empty()
    */
   [[deprecated("Use for_each() or loop_until() free function instead.")]] uint64_t unpack_size() {
      uint64_t num;
      varuint64_from_bin(num, this->bin);
      return num;
   }

   [[deprecated("Use for_each() or loop_until() free function instead.")]] void unpack_next(T& obj) {
      eosio::from_bin(obj, this->bin);
   }

   [[deprecated("Use for_each() or loop_until() free function instead.")]] T unpack_next() {
      T obj;
      this->unpack_next(obj);
      return obj;
   }

   template <typename U>
   friend opaque<U> as_opaque(input_stream bin);
};

template <typename T>
constexpr const char* get_type_name(opaque<T>*) {
   return "bytes";
}

template <typename T, typename S>
void from_bin(opaque<T>& obj, S& stream) {
   obj.from(stream);
}

template <typename T, typename S>
void to_bin(const opaque<T>& obj, S& stream) {
   obj.to_bin(stream);
}

template <typename U>
opaque<U> as_opaque(input_stream bin) {
   opaque<U> result;
   result.bin = bin;
   return result;
}

template <typename T, typename U>
std::enable_if_t<std::is_base_of_v<U, T>, bool> unpack(opaque<T> opq, U& obj) {
   if (opq.empty())
      return false;
   input_stream bin = opq.get();
   eosio::from_bin(obj, bin);
   return true;
}

template <typename T, typename Predicate>
void loop_until(opaque<std::vector<T>> opq, Predicate&& f) {
   if (opq.empty())
      return;
   input_stream bin = opq.get();
   uint64_t     num;
   varuint64_from_bin(num, bin);
   for (uint64_t i = 0; i < num; ++i) {
      T obj;
      eosio::from_bin(obj, bin);
      if (f(std::move(obj)))
         return;
   }
}

template <typename T, typename UnaryFunction>
void for_each(opaque<std::vector<T>> opq, UnaryFunction&& f) {
   loop_until(opq, [&f](auto&& x) {
      f(std::forward<decltype(x)>(x));
      return false;
   });
}

} // namespace eosio
