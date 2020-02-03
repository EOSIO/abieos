#include <eosio/abi.hpp>
#include "abieos.hpp"

using namespace eosio;

namespace {

template <int i>
bool ends_with(const std::string& s, const char (&suffix)[i]) {
    return s.size() >= i - 1 && !strcmp(s.c_str() + s.size() - (i - 1), suffix);
}

template <typename T>
struct abi_serializer_impl : abi_serializer {
    result<void> json_to_bin(::abieos::jvalue_to_bin_state& state, bool allow_extensions, const abi_type* type,
                             bool start) const override {
        return ::abieos::json_to_bin((T*)nullptr, state, allow_extensions, type, start);
    }
    result<void> json_to_bin(::abieos::json_to_bin_state& state, bool allow_extensions, const abi_type* type,
                             bool start) const override {
        return ::abieos::json_to_bin((T*)nullptr, state, allow_extensions, type, start);
    }
    result<void> bin_to_json(::abieos::bin_to_json_state& state, bool allow_extensions, const abi_type* type,
                             bool start) const override {
        return ::abieos::bin_to_json((T*)nullptr, state, allow_extensions, type, start);
    }
};

template <typename T>
constexpr auto abi_serializer_for = abi_serializer_impl<T>{};

result<abi_type::alias> resolve(std::map<std::string, abi_type>& abi_types, const abi_type::alias_def* type, int depth);

template<typename... T, typename... A>
bool holds_any_alternative(const std::variant<A...>& v) {
   return (... || std::holds_alternative<T>(v));
}

template <typename F>
constexpr void for_each_abi_type(F f) {
    static_assert(sizeof(float) == 4);
    static_assert(sizeof(double) == 8);

    using namespace ::abieos;
    f("bool", (bool*)nullptr);
    f("int8", (int8_t*)nullptr);
    f("uint8", (uint8_t*)nullptr);
    f("int16", (int16_t*)nullptr);
    f("uint16", (uint16_t*)nullptr);
    f("int32", (int32_t*)nullptr);
    f("uint32", (uint32_t*)nullptr);
    f("int64", (int64_t*)nullptr);
    f("uint64", (uint64_t*)nullptr);
    f("int128", (int128*)nullptr);
    f("uint128", (uint128*)nullptr);
    f("varuint32", (varuint32*)nullptr);
    f("varint32", (varint32*)nullptr);
    f("float32", (float*)nullptr);
    f("float64", (double*)nullptr);
    f("float128", (float128*)nullptr);
    f("time_point", (time_point*)nullptr);
    f("time_point_sec", (time_point_sec*)nullptr);
    f("block_timestamp_type", (block_timestamp*)nullptr);
    f("name", (name*)nullptr);
    f("bytes", (bytes*)nullptr);
    f("string", (std::string*)nullptr);
    f("checksum160", (checksum160*)nullptr);
    f("checksum256", (checksum256*)nullptr);
    f("checksum512", (checksum512*)nullptr);
    f("public_key", (public_key*)nullptr);
    f("private_key", (private_key*)nullptr);
    f("signature", (signature*)nullptr);
    f("symbol", (symbol*)nullptr);
    f("symbol_code", (symbol_code*)nullptr);
    f("asset", (asset*)nullptr);
}

result<abi_type*> get_type(std::map<std::string, abi_type>& abi_types,
                           const std::string& name, int depth) {
    if (depth >= 32)
        return abi_error::recursion_limit_reached;
    auto it = abi_types.find(name);
    if (it == abi_types.end()) {
        if (ends_with(name, "?")) {
            OUTCOME_TRY(base, get_type(abi_types, name.substr(0, name.size() - 1), depth + 1));
            if (holds_any_alternative<abi_type::optional, abi_type::array, abi_type::extension>(base->_data))
                return abi_error::invalid_nesting;
            auto [iter, success] = abi_types.try_emplace(name, name, abi_type::optional{base}, &abi_serializer_for< ::abieos::pseudo_optional>);
            return &iter->second;
        } else if (ends_with(name, "[]")) {
            OUTCOME_TRY(element, get_type(abi_types, name.substr(0, name.size() - 2), depth + 1));
            if (holds_any_alternative<abi_type::optional, abi_type::array, abi_type::extension>(element->_data))
                return abi_error::invalid_nesting;
            auto [iter, success] = abi_types.try_emplace(name, name, abi_type::array{element}, &abi_serializer_for< ::abieos::pseudo_array>);
            return &iter->second;
        } else if (ends_with(name, "$")) {
            OUTCOME_TRY(base, get_type(abi_types, name.substr(0, name.size() - 1), depth + 1));
            if (std::holds_alternative<abi_type::extension>(base->_data))
                return abi_error::invalid_nesting;
            auto [iter, success] = abi_types.try_emplace(name, name, abi_type::extension{base}, &abi_serializer_for< ::abieos::pseudo_extension>);
            return &iter->second;
        } else
            return abi_error::unknown_type;
    }

    // resolve aliases
    if (auto* alias = std::get_if<abi_type::alias>(&it->second._data)) {
        return alias->type;
    } else if(auto* alias = std::get_if<const abi_type::alias_def*>(&it->second._data)) {
        OUTCOME_TRY(base, resolve(abi_types, *alias, depth));
        it->second._data = base;
        return base.type;
    }

    return &it->second;
}

result<abi_type::struct_> resolve(std::map<std::string, abi_type>& abi_types, const struct_def* type, int depth) {
    if (depth >= 32)
        return abi_error::recursion_limit_reached;
    abi_type::struct_ result;
    if (!type->base.empty()) {
        OUTCOME_TRY(base, get_type(abi_types, type->base, depth + 1));

        if(auto* base_def = std::get_if<const struct_def*>(&base->_data)) {
            OUTCOME_TRY(b, resolve(abi_types, *base_def, depth + 1));
            base->_data = std::move(b);
        }
        if(auto* b = std::get_if<abi_type::struct_>(&base->_data)) {
            result.fields = b->fields;
        } else {
            return abi_error::base_not_a_struct;
        }
    }
    for (auto& field : type->fields) {
        OUTCOME_TRY(t, get_type(abi_types, field.type, depth + 1));
        result.fields.push_back(abi_field{field.name, t});
    }
    return std::move(result);
}


result<abi_type::variant> resolve(std::map<std::string, abi_type>& abi_types, const variant_def* type, int depth) {
    if (depth >= 32)
        return abi_error::recursion_limit_reached;
    abi_type::variant result;
    for (const std::string& field : type->types) {
        OUTCOME_TRY(t, get_type(abi_types, field, depth + 1));
        result.push_back({field, t});
    }
    return std::move(result);
}

result<abi_type::alias> resolve(std::map<std::string, abi_type>& abi_types, const abi_type::alias_def* type, int depth) {
    OUTCOME_TRY(t, get_type(abi_types, *type, depth + 1));
    if(std::holds_alternative<abi_type::extension>(t->_data))
        return abi_error::extension_typedef;
    return abi_type::alias{t};
}

struct fill_t {
   std::map<std::string, abi_type>& abi_types;
   abi_type& type;
   int depth;
   template<typename T>
   auto operator()(T& t) -> result<std::void_t<decltype(resolve(abi_types, t, depth))>> {
      OUTCOME_TRY(x, resolve(abi_types, t, depth));
      type._data = std::move(x);
      return outcome::success();
   }
   template<typename T>
   auto operator()(const T& t) -> result<void> {
      return outcome::success();
   }
};

result<void> fill(std::map<std::string, abi_type>& abi_types, abi_type& type, int depth) {
   return std::visit(fill_t{abi_types, type, depth}, type._data);
}

}


result<const abi_type*> eosio::abi::get_type(const std::string& name) {
   return result<const abi_type*>(::get_type(abi_types, name, 0));
}

result<void> eosio::convert(const abi_def& abi, eosio::abi& c) {
    for (auto& a : abi.actions)
        c.action_types[a.name] = a.type;
    for (auto& t : abi.tables)
        c.table_types[t.name] = t.type;
    for_each_abi_type([&](const char* name, auto* p) {
        c.abi_types.try_emplace(name, name, abi_type::builtin{}, &abi_serializer_for<std::decay_t<decltype(*p)>>);
    });
    {
        c.abi_types.try_emplace("extended_asset", "extended_asset",
                                abi_type::struct_{nullptr, {{"quantity", &c.abi_types.find("asset")->second},
                                                            {"contract", &c.abi_types.find("name")->second}}},
                                &abi_serializer_for<::abieos::pseudo_object>);
    }

    for (auto& t : abi.types) {
        if (t.new_type_name.empty())
            return abi_error::missing_name;
        auto [_, inserted] = c.abi_types.try_emplace(t.new_type_name, t.new_type_name, &t.type, nullptr);
        if (!inserted)
            return abi_error::redefined_type;
    }
    for (auto& s : abi.structs) {
        if (s.name.empty())
            return abi_error::missing_name;
        auto [it, inserted] = c.abi_types.try_emplace(s.name, s.name, &s, &abi_serializer_for<::abieos::pseudo_object>);
        if (!inserted)
            return abi_error::redefined_type;
    }
    for (auto& v : abi.variants.value) {
        if (v.name.empty())
            return abi_error::missing_name;
        auto [it, inserted] = c.abi_types.try_emplace(v.name, v.name, &v, &abi_serializer_for<::abieos::pseudo_variant>);
        if (!inserted)
            return abi_error::redefined_type;
    }
    for (auto& [_, t] : c.abi_types) {
        OUTCOME_TRY(fill(c.abi_types, t, 0));
    }
    return outcome::success();
}

extern const abi_serializer* const eosio::object_abi_serializer = &abi_serializer_for< ::abieos::pseudo_object>;
extern const abi_serializer* const eosio::variant_abi_serializer = &abi_serializer_for< ::abieos::pseudo_variant>;
extern const abi_serializer* const eosio::array_abi_serializer = &abi_serializer_for< ::abieos::pseudo_array>;
extern const abi_serializer* const eosio::extension_abi_serializer = &abi_serializer_for< ::abieos::pseudo_extension>;
extern const abi_serializer* const eosio::optional_abi_serializer = &abi_serializer_for< ::abieos::pseudo_optional>;
