#pragma once

#include <functional>
#include <string>
#include <map>
#include <vector>
#include <variant>
#include <eosio/types.hpp>
#include <eosio/name.hpp>

namespace eosio {

enum class abi_error {
   no_error,

   recursion_limit_reached,
   invalid_nesting,
   unknown_type,
   missing_name,
   redefined_type,
   base_not_a_struct,
   extension_typedef,
   bad_abi
};

}

namespace std {
template <>
struct is_error_code_enum<eosio::abi_error> : true_type {};
}

namespace eosio {

struct abi_serializer;

class abi_error_category_type : public std::error_category {
 public:
   const char* name() const noexcept override final { return "ABIError"; }

   std::string message(int c) const override final {
      switch(static_cast<abi_error>(c)) {
      default: return "unknown";
      }
   }
};

inline const abi_error_category_type& abi_error_category() {
   static abi_error_category_type c;
   return c;
}

inline std::error_code make_error_code(abi_error e) { return { static_cast<int>(e), abi_error_category() }; }

template <typename T>
struct might_not_exist {
    T value{};
};

template <typename T, typename S>
result<void> from_bin(might_not_exist<T>& obj, S& stream) {
    if (stream.remaining())
        return from_bin(obj.value, stream);
    return eosio::outcome::success();
}

template <typename T, typename S>
result<void> from_json(might_not_exist<T>& obj, S& stream) {
    return from_json(obj.value, stream);
}

using extensions_type = std::vector<std::pair<uint16_t, std::vector<char>>>;

struct type_def {
    std::string new_type_name{};
    std::string type{};
};

EOSIO_REFLECT(type_def, new_type_name, type);

struct field_def {
    std::string name{};
    std::string type{};
};

EOSIO_REFLECT(field_def, name, type);

struct struct_def {
    std::string name{};
    std::string base{};
    std::vector<field_def> fields{};
};

EOSIO_REFLECT(struct_def, name, base, fields);

struct action_def {
    eosio::name name{};
    std::string type{};
    std::string ricardian_contract{};
};

EOSIO_REFLECT(action_def, name, type, ricardian_contract);

struct table_def {
    eosio::name name{};
    std::string index_type{};
    std::vector<std::string> key_names{};
    std::vector<std::string> key_types{};
    std::string type{};
};

EOSIO_REFLECT(table_def, name, index_type, key_names, key_types, type);

struct clause_pair {
    std::string id{};
    std::string body{};
};

EOSIO_REFLECT(clause_pair, id, body);

struct error_message {
    uint64_t error_code{};
    std::string error_msg{};
};

EOSIO_REFLECT(error_message, error_code, error_msg);

struct variant_def {
    std::string name{};
    std::vector<std::string> types{};
};

EOSIO_REFLECT(variant_def, name, types);

struct abi_def {
    std::string version{};
    std::vector<type_def> types{};
    std::vector<struct_def> structs{};
    std::vector<action_def> actions{};
    std::vector<table_def> tables{};
    std::vector<clause_pair> ricardian_clauses{};
    std::vector<error_message> error_messages{};
    extensions_type abi_extensions{};
    might_not_exist<std::vector<variant_def>> variants{};
};

EOSIO_REFLECT(abi_def, version, types, structs, actions, tables, ricardian_clauses, error_messages, abi_extensions,
              variants);

struct abi_type;

struct abi_field {
    std::string name;
    const abi_type* type;
};

struct abi_type {
    std::string name;

    struct builtin {};
    using alias_def = std::string;
    struct alias { abi_type* type; };
    struct optional { abi_type* type; };
    struct extension { abi_type* type; };
    struct array { abi_type* type; };
    struct struct_ {
        abi_type* base = nullptr;
        std::vector<abi_field> fields;
    };
    using variant = std::vector<abi_field>;
    std::variant<builtin, const alias_def*, const struct_def*, const variant_def*, alias, optional, extension, array, struct_, variant> _data;
    const abi_serializer* ser = nullptr;

    template<typename T>
    abi_type(std::string name, T&& arg, const abi_serializer* ser)
        : name(std::move(name)), _data(std::move(arg)), ser(ser) {}
    abi_type(const abi_type&) = delete;
    abi_type& operator=(const abi_type&) = delete;

    // result<void> json_to_bin(std::vector<char>& bin, std::string_view json);
    const abi_type* optional_of() const {
       if(auto* t = std::get_if<optional>(&_data)) return t->type;
       else return nullptr;
    }
    const abi_type* extension_of() const {
       if(auto* t = std::get_if<extension>(&_data)) return t->type;
       else return nullptr;
    }
    const abi_type* array_of() const {
       if(auto* t = std::get_if<array>(&_data)) return t->type;
       else return nullptr;
    }
    const struct_* as_struct() const {
       return std::get_if<struct_>(&_data);
    }
    const variant* as_variant() const {
       return std::get_if<variant>(&_data);
    }

    result<std::string> bin_to_json(input_stream& bin, std::function<void()> f = []{}) const;
    result<std::vector<char>> json_to_bin(std::string_view json, std::function<void()> f = []{}) const;
    result<std::vector<char>> json_to_bin_reorderable(std::string_view json, std::function<void()> f = []{}) const;
};

struct abi {
    std::map<eosio::name, std::string> action_types;
    std::map<eosio::name, std::string> table_types;
    std::map<std::string, abi_type> abi_types;
    result<const abi_type*> get_type(const std::string& name);

    // Adds a type to the abi.  Has no effect if the type is already present.
    // If the type is a struct, all members will be added recursively.
    // Exception Safety: basic. If add_type fails, some objects may have
    // an incomplete list of fields.
    template<typename T>
    result<abi_type*> add_type();
};

result<void> convert(const abi_def& def, abi&);
result<void> convert(const abi& def, abi_def&);

extern const abi_serializer* const object_abi_serializer;
extern const abi_serializer* const variant_abi_serializer;
extern const abi_serializer* const array_abi_serializer;
extern const abi_serializer* const extension_abi_serializer;
extern const abi_serializer* const optional_abi_serializer;

template<typename T>
auto add_type(abi& a, T*) -> std::enable_if_t<reflection::has_for_each_field_v<T>, result<abi_type*>> {
   std::string name = get_type_name((T*)nullptr);
   auto [iter, inserted] = a.abi_types.try_emplace(name, name, abi_type::struct_{}, object_abi_serializer);
   if(!inserted)
      return &iter->second;
   auto& s = std::get<abi_type::struct_>(iter->second._data);
   result<void> okay = outcome::success();
   for_each_field<T>([&](const char* name, auto&& member){
      if(okay) {
         auto member_type = a.add_type<std::decay_t<decltype(member((T*)nullptr))>>();
         if(member_type) {
            s.fields.push_back({name, member_type.value()});
         } else {
            okay = member_type.error();
         }
      }
   });
   return &iter->second;
}

template<typename T>
auto add_type(abi& a, T* t) -> std::enable_if_t<!reflection::has_for_each_field_v<T>, result<abi_type*>> {
   auto iter = a.abi_types.find(get_type_name(t));
   if(iter != a.abi_types.end()) return &iter->second;
   else return abi_error::unknown_type;
}

template<typename T>
result<abi_type*> add_type(abi& a, std::vector<T>*) {
   OUTCOME_TRY(element_type, a.add_type<T>());
   if (element_type->optional_of() || element_type->array_of() || element_type->extension_of())
      return abi_error::invalid_nesting;
   std::string name = get_type_name((std::vector<T>*)nullptr);
   auto [iter, inserted] = a.abi_types.try_emplace(name, name, abi_type::array{element_type}, array_abi_serializer);
   return &iter->second;
}

template<typename... T>
result<abi_type*> add_type(abi& a, std::variant<T...>*) {
   abi_type::variant types;
   result<void> okay = outcome::success();
   ([&](auto* t) {
      if(okay) {
         if(auto type = add_type(a, t)) {
            types.push_back({type.value()->name, type.value()});
         } else {
            okay = type.error();
         }
      }
   }((T*)nullptr), ...);
   OUTCOME_TRY(okay);
   std::string name = get_type_name((std::variant<T...>*)nullptr);

   auto [iter, inserted] = a.abi_types.try_emplace(name, name, std::move(types), variant_abi_serializer);
   return &iter->second;
}

template<typename T>
result<abi_type*> add_type(abi& a, std::optional<T>*) {
   OUTCOME_TRY(element_type, a.add_type<T>());
   if (element_type->optional_of() || element_type->array_of() || element_type->extension_of())
      return abi_error::invalid_nesting;
   std::string name = get_type_name((std::optional<T>*)nullptr);
   auto [iter, inserted] = a.abi_types.try_emplace(name, name, abi_type::optional{element_type}, optional_abi_serializer);
   return &iter->second;
}

template<typename T>
result<abi_type*> add_type(abi& a, might_not_exist<T>*) {
   OUTCOME_TRY(element_type, a.add_type<T>());
   if (element_type->extension_of())
      return abi_error::invalid_nesting;
   std::string name = element_type->name + "$";
   auto [iter, inserted] = a.abi_types.try_emplace(name, name, abi_type::extension{element_type}, extension_abi_serializer);
   return &iter->second;
}

template<typename T>
result<abi_type*> abi::add_type() {
   using eosio::add_type;
   return add_type(*this, (T*)nullptr);
}

}
