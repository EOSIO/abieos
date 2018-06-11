#include <array>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

#include "rapidjson/reader.h"

namespace abieos {

template <typename T>
inline constexpr bool is_vector_v = false;

template <typename T>
inline constexpr bool is_vector_v<std::vector<T>> = true;

template <typename T>
inline constexpr bool is_pair_v = false;

template <typename First, typename Second>
inline constexpr bool is_pair_v<std::pair<First, Second>> = true;

template <auto P>
struct member_ptr {};

template <class C, typename M>
C* class_from_void(M C::*, void* v) {
    return reinterpret_cast<C*>(v);
}

template <auto P>
auto& member_from_void(member_ptr<P>, void* p) {
    return class_from_void(P, p)->*P;
}

struct bin_to_native_state;
struct json_to_native_state;
enum class json_event;

struct serializer {
    virtual bool bin_to_native(bin_to_native_state&, void*, bool) const = 0;
    virtual bool json_to_native(void*, json_to_native_state&, json_event, bool) const = 0;
};

struct field_serializer_methods {
    virtual bool bin_to_native(bin_to_native_state&, void*, bool) const = 0;
    virtual bool json_to_native(void*, json_to_native_state&, json_event, bool) const = 0;
};

struct field_serializer {
    std::string_view name = "<unknown>";
    bool required = true;
    const field_serializer_methods* methods = nullptr;
};

struct stack_entry {
    void* obj = nullptr;
    serializer* ser = nullptr;
    int position = 0;
};

template <typename T>
struct root_object_wrapper {
    T& obj;
};

struct bin_to_native_state {
    std::vector<stack_entry> stack;
};

template <typename T>
bool bin_to_native(T& obj);
template <typename T>
bool bin_to_native(bin_to_native_state& state, T& obj, bool start);
bool bin_to_native(bin_to_native_state& state, std::string& obj, bool start);
template <typename T>
bool bin_to_native(bin_to_native_state& state, root_object_wrapper<T>& wrapper, bool start);

enum class json_event {
    received_null,         // 0
    received_bool,         // 1
    received_uint64,       // 2
    received_int64,        // 3
    received_double,       // 4
    received_string,       // 5
    received_start_object, // 6
    received_key,          // 7
    received_end_object,   // 8
    received_start_array,  // 9
    received_end_array,    // 10
};

struct json_to_native_state : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, json_to_native_state> {
    std::vector<stack_entry> stack;

    bool value_bool = 0;
    uint64_t value_uint64 = 0;
    int64_t value_int64 = 0;
    double value_double = 0;
    std::string value_string{};
    std::string key{};

    bool process(json_event event);

    bool Null() { return process(json_event::received_null); }
    bool Bool(bool v) {
        value_bool = v;
        return process(json_event::received_bool);
    }
    bool Int(int v) { return Int64(v); }
    bool Uint(unsigned v) { return Uint64(v); }
    bool Int64(int64_t v) {
        value_int64 = v;
        return process(json_event::received_int64);
    }
    bool Uint64(uint64_t v) {
        value_uint64 = v;
        return process(json_event::received_uint64);
    }
    bool Double(double v) {
        value_double = v;
        return process(json_event::received_double);
    }
    bool String(const char* v, rapidjson::SizeType length, bool) {
        value_string = {v, length};
        return process(json_event::received_string);
    }
    bool StartObject() { return process(json_event::received_start_object); }
    bool Key(const char* v, rapidjson::SizeType length, bool) {
        key = {v, length};
        return process(json_event::received_key);
    }
    bool EndObject(rapidjson::SizeType) { return process(json_event::received_end_object); }
    bool StartArray() { return process(json_event::received_start_array); }
    bool EndArray(rapidjson::SizeType) { return process(json_event::received_end_array); }
}; // json_to_native_state

template <typename T>
auto json_to_native(T& obj, json_to_native_state& state, json_event event, bool start)
    -> std::enable_if_t<std::is_arithmetic_v<T>, bool>;
template <typename T>
auto json_to_native(T& obj, json_to_native_state& state, json_event event, bool start)
    -> std::enable_if_t<std::is_class_v<T>, bool>;
template <typename T>
auto json_to_native(std::vector<T>& obj, json_to_native_state& state, json_event event, bool start);
template <typename First, typename Second>
auto json_to_native(std::pair<First, Second>& obj, json_to_native_state& state, json_event event, bool start);
bool json_to_native(std::string& obj, json_to_native_state& state, json_event event, bool start);
template <typename T>
bool json_to_native(root_object_wrapper<T>& obj, json_to_native_state& state, json_event event, bool start);

struct hex_bytes {};

inline bool bin_to_native(bin_to_native_state& state, hex_bytes& obj, bool start) { return true; }
inline bool json_to_native(hex_bytes& obj, json_to_native_state& state, json_event event, bool start) { return false; }

struct name {
    uint64_t value = 0;
};

inline bool bin_to_native(bin_to_native_state& state, name& obj, bool start) {
    return bin_to_native(state, obj.value, start);
}

inline bool json_to_native(name& obj, json_to_native_state& state, json_event event, bool start) {
    printf("???? %d\n", event);
    if (event == json_event::received_string) {
        printf("%*sname: %s\n", int(state.stack.size() * 4), "", state.value_string.c_str());
        // todo
        return true;
    } else
        return false;
}

using action_name = name;
using field_name = name;
using table_name = name;
using type_name = name;

using extensions_type = std::vector<std::pair<uint16_t, hex_bytes>>;

struct type_def {
    type_name new_type_name{};
    type_name type{};
};

template <typename F>
constexpr void for_each_field(type_def*, F f) {
    f("new_type_name", member_ptr<&type_def::new_type_name>{}, true);
    f("type", member_ptr<&type_def::type>{}, true);
}

struct field_def {
    field_name name{};
    type_name type{};
};

template <typename F>
constexpr void for_each_field(field_def*, F f) {
    f("name", member_ptr<&field_def::name>{}, true);
    f("type", member_ptr<&field_def::type>{}, true);
}

struct struct_def {
    type_name name{};
    type_name base{};
    std::vector<field_def> fields{};
};

template <typename F>
constexpr void for_each_field(struct_def*, F f) {
    f("name", member_ptr<&struct_def::name>{}, true);
    f("base", member_ptr<&struct_def::base>{}, false);
    f("fields", member_ptr<&struct_def::fields>{}, false);
}

struct action_def {
    action_name name{};
    type_name type{};
    std::string ricardian_contract{};
};

template <typename F>
constexpr void for_each_field(action_def*, F f) {
    f("name", member_ptr<&action_def::name>{}, true);
    f("type", member_ptr<&action_def::type>{}, true);
    f("ricardian_contract", member_ptr<&action_def::ricardian_contract>{}, false);
}

struct table_def {
    table_name name{};
    type_name index_type{};
    std::vector<field_name> key_names{};
    std::vector<type_name> key_types{};
    type_name type{};
};

template <typename F>
constexpr void for_each_field(table_def*, F f) {
    f("name", member_ptr<&table_def::name>{}, true);
    f("index_type", member_ptr<&table_def::index_type>{}, true);
    f("key_names", member_ptr<&table_def::key_names>{}, false);
    f("key_types", member_ptr<&table_def::key_types>{}, false);
    f("type", member_ptr<&table_def::key_types>{}, true);
}

struct clause_pair {
    std::string id{};
    std::string body{};
};

template <typename F>
constexpr void for_each_field(clause_pair*, F f) {
    f("id", member_ptr<&clause_pair::id>{}, true);
    f("body", member_ptr<&clause_pair::body>{}, true);
}

struct error_message {
    uint64_t error_code{};
    std::string error_msg{};
};

template <typename F>
constexpr void for_each_field(error_message*, F f) {
    f("error_code", member_ptr<&error_message::error_code>{}, true);
    f("error_msg", member_ptr<&error_message::error_msg>{}, true);
}

struct abi_def {
    std::string version{"eosio::abi/1.0"};
    std::vector<type_def> types{};
    std::vector<struct_def> structs{};
    std::vector<action_def> actions{};
    std::vector<table_def> tables{};
    std::vector<clause_pair> ricardian_clauses{};
    std::vector<error_message> error_messages{};
    extensions_type abi_extensions{};
};

template <typename F>
constexpr void for_each_field(abi_def*, F f) {
    f("version", member_ptr<&abi_def::version>{}, false);
    f("types", member_ptr<&abi_def::types>{}, false);
    f("structs", member_ptr<&abi_def::structs>{}, false);
    f("actions", member_ptr<&abi_def::actions>{}, false);
    f("tables", member_ptr<&abi_def::tables>{}, false);
    f("ricardian_clauses", member_ptr<&abi_def::ricardian_clauses>{}, false);
    f("error_messages", member_ptr<&abi_def::error_messages>{}, false);
    f("abi_extensions", member_ptr<&abi_def::abi_extensions>{}, false);
}

template <typename F>
constexpr void for_each_type(F f) {
    // These must remain in lexicographical order
    f("abi_def", (abi_def*)nullptr);
    f("action_def", (action_def*)nullptr);
    f("clause_pair", (clause_pair*)nullptr);
    f("error_message", (error_message*)nullptr);
    f("field_def", (field_def*)nullptr);
    f("name", (name*)nullptr);
    f("struct_def", (struct_def*)nullptr);
    f("table_def", (table_def*)nullptr);
    f("type_def", (type_def*)nullptr);
}

template <typename T>
struct serializer_impl : serializer {
    bool bin_to_native(bin_to_native_state& state, void* v, bool start) const override {
        return ::abieos::bin_to_native(state, *reinterpret_cast<T*>(v), start);
    }
    bool json_to_native(void* v, json_to_native_state& state, json_event event, bool start) const override {
        return ::abieos::json_to_native(*reinterpret_cast<T*>(v), state, event, start);
    }
};

template <typename T>
auto serializer_for = serializer_impl<T>{};

inline constexpr auto create_all_serializers() {
    constexpr auto num_types = [] {
        int num_types = 0;
        for_each_type([&](auto, auto) { ++num_types; });
        return num_types;
    }();
    std::array<const serializer*, num_types> all_serializers({});
    int i = 0;
    for_each_type(
        [&](auto name, auto* p) { all_serializers[i++] = &serializer_for<std::remove_reference_t<decltype(*p)>>; });
    return all_serializers;
}

inline constexpr auto all_serializers = create_all_serializers();

template <typename member_ptr>
constexpr auto create_field_serializer_methods_impl() {
    struct impl : field_serializer_methods {
        bool bin_to_native(bin_to_native_state& state, void* v, bool start) const override {
            return ::abieos::bin_to_native(state, member_from_void(member_ptr{}, v), start);
        }
        bool json_to_native(void* v, json_to_native_state& state, json_event event, bool start) const override {
            return ::abieos::json_to_native(member_from_void(member_ptr{}, v), state, event, start);
        }
    };
    return impl{};
}

template <typename member_ptr>
constexpr auto field_serializer_methods_impl = create_field_serializer_methods_impl<member_ptr>();

template <typename T>
constexpr auto create_field_serializers() {
    constexpr auto num_fields = ([&]() constexpr {
        int num_fields = 0;
        for_each_field((T*)nullptr, [&](auto, auto, bool) { ++num_fields; });
        return num_fields;
    }());
    std::array<field_serializer, num_fields> fields;
    int i = 0;
    for_each_field((T*)nullptr, [&](auto* name, auto member_ptr, auto required) {
        fields[i++] = {name, required, &field_serializer_methods_impl<decltype(member_ptr)>};
    });
    return fields;
}

template <typename T>
constexpr inline auto field_serializers = create_field_serializers<T>();

template <typename State, typename T, typename F>
bool serialize_class(State& state, T& obj, bool start, F f) {
    if (start) {
        printf("%*s{ %d fields\n", int(state.stack.size() * 4), "", int(field_serializers<T>.size()));
        state.stack.push_back({&obj, &serializer_for<T>});
        return true;
    }
    auto& stack_entry = state.stack.back();
    if (stack_entry.position < field_serializers<T>.size()) {
        auto& field_ser = field_serializers<T>[stack_entry.position];
        printf("%*sfield %d/%d: %s\n", int(state.stack.size() * 4), "", int(stack_entry.position),
               int(field_serializers<T>.size()), std::string{field_serializers<T>[stack_entry.position].name}.c_str());
        ++stack_entry.position;
        return f(field_ser);
    } else {
        printf("%*s}\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return true;
    }
}

template <typename State, typename T, typename F>
bool serialize_vector(State& state, std::vector<T>& v, bool start, F f) {
    if (start) {
        v.resize(3); // !!!
        printf("%*s[ %d items\n", int(state.stack.size() * 4), "", int(v.size()));
        state.stack.push_back({&v, &serializer_for<std::vector<T>>});
        return true;
    }
    auto& stack_entry = state.stack.back();
    if (stack_entry.position < v.size()) {
        printf("%*sitem %d/%d\n", int(state.stack.size() * 4), "", int(stack_entry.position), int(v.size()));
        return f(v[stack_entry.position++]);
    } else {
        printf("%*s]\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return true;
    }
    return true;
}

template <typename State, typename First, typename Second, typename F>
bool serialize_pair(State& state, std::pair<First, Second>& v, bool start, F f) {
    if (start) {
        printf("%*s[ pair\n", int(state.stack.size() * 4), "");
        state.stack.push_back({&v, &serializer_for<std::pair<First, Second>>});
        return true;
    }
    auto& stack_entry = state.stack.back();
    if (stack_entry.position == 0) {
        printf("%*sitem 0/1\n", int(state.stack.size() * 4), "");
        ++stack_entry.position;
        return f(v.first);
    } else if (stack_entry.position == 1) {
        printf("%*sitem 1/1\n", int(state.stack.size() * 4), "");
        ++stack_entry.position;
        return f(v.second);
    } else {
        printf("%*s]\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return true;
    }
    return true;
}

template <typename T>
bool bin_to_native(bin_to_native_state& state, root_object_wrapper<T>& wrapper, bool start) {
    return false;
}

template <typename T>
bool bin_to_native(bin_to_native_state& state, T& obj, bool start) {
    static_assert(std::is_arithmetic_v<T> || std::is_class_v<T>);
    if constexpr (std::is_arithmetic_v<T>) {
        // !!!
    } else if constexpr (is_vector_v<T>) {
        return serialize_vector(state, obj, start, [&](auto& item) { //
            return serializer_for<std::decay_t<decltype(item)>>.bin_to_native(state, &item, true);
        });
    } else if constexpr (is_pair_v<T>) {
        return serialize_pair(state, obj, start, [&](auto& item) { //
            return serializer_for<std::decay_t<decltype(item)>>.bin_to_native(state, &item, true);
        });
    } else if constexpr (std::is_class_v<T>) {
        return serialize_class(state, obj, start, [&](auto& field_ser) { //
            return field_ser.methods->bin_to_native(state, &obj, true);
        });
    }
    return true;
}

inline bool bin_to_native(bin_to_native_state& state, std::string& obj, bool) { return true; }

template <typename T>
bool bin_to_native(T& obj) {
    bin_to_native_state state;
    if (!bin_to_native(state, obj, true))
        return false;
    while (!state.stack.empty()) {
        auto& x = state.stack.back();
        if (!x.ser->bin_to_native(state, x.obj, false))
            return false;
    }
    return true;
}

inline bool json_to_native_state::process(json_event event) {
    if (stack.empty())
        return false;
    auto& x = stack.back();
    printf("> %d\n", event);
    return x.ser->json_to_native(x.obj, *this, event, false);
}

template <typename T>
bool json_to_native(T& obj, std::string_view json) {
    std::string mutable_json{json};
    json_to_native_state state;
    root_object_wrapper<T> wrapper{obj};
    if (!json_to_native(wrapper, state, json_event::received_start_object, true))
        return false;
    rapidjson::Reader reader;
    rapidjson::InsituStringStream ss(mutable_json.data());
    return reader.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag |
                        rapidjson::kParseFullPrecisionFlag>(ss, state);
}

template <typename T>
auto json_to_native(T& obj, json_to_native_state& state, json_event event, bool start)
    -> std::enable_if_t<std::is_arithmetic_v<T>, bool> {

    if (event == json_event::received_bool)
        obj = state.value_bool;
    else if (event == json_event::received_uint64)
        obj = state.value_uint64;
    else if (event == json_event::received_int64)
        obj = state.value_int64;
    else if (event == json_event::received_double)
        obj = state.value_double;
    else
        return false;
    return true;
}

template <typename T>
bool json_to_native(root_object_wrapper<T>& wrapper, json_to_native_state& state, json_event event, bool start) {
    if (start) {
        state.stack.push_back({&wrapper, &serializer_for<root_object_wrapper<T>>});
        return true;
    } else if (event == json_event::received_start_object)
        return json_to_native(wrapper.obj, state, json_event::received_start_object, true);
    else
        return false;
}

template <typename T>
auto json_to_native(T& obj, json_to_native_state& state, json_event event, bool start)
    -> std::enable_if_t<std::is_class_v<T>, bool> {

    if (start) {
        if (event != json_event::received_start_object)
            return false;
        printf("%*s{ %d fields\n", int(state.stack.size() * 4), "", int(field_serializers<T>.size()));
        state.stack.push_back({&obj, &serializer_for<T>});
        return true;
    } else if (event == json_event::received_end_object) {
        printf("%*s}\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return true;
    }
    auto& stack_entry = state.stack.back();
    if (event == json_event::received_key) {
        stack_entry.position = 0;
        while (stack_entry.position < field_serializers<T>.size() &&
               field_serializers<T>[stack_entry.position].name != state.key)
            ++stack_entry.position;
        printf("???? %d %s\n", stack_entry.position, state.key.c_str());
        if (stack_entry.position >= field_serializers<T>.size())
            return false; // TODO: eat unknown subtree
        return true;
    } else if (stack_entry.position < field_serializers<T>.size()) {
        auto& field_ser = field_serializers<T>[stack_entry.position];
        printf("%*sfield %d/%d: %s (event %d)\n", int(state.stack.size() * 4), "", int(stack_entry.position),
               int(field_serializers<T>.size()), std::string{field_ser.name}.c_str(), event);
        return field_ser.methods->json_to_native(&obj, state, event, true);
    } else {
        return true;
    }
    return true;
}

template <typename T>
auto json_to_native(std::vector<T>& v, json_to_native_state& state, json_event event, bool start) {
    if (start) {
        if (event != json_event::received_start_array)
            return false;
        printf("%*s[\n", int(state.stack.size() * 4), "");
        state.stack.push_back({&v, &serializer_for<std::vector<T>>});
        return true;
    } else if (event == json_event::received_end_array) {
        printf("%*s]\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return true;
    }
    printf("%*sitem %d (event %d)\n", int(state.stack.size() * 4), "", int(v.size()), event);
    v.emplace_back();
    return json_to_native(v.back(), state, event, true);
}

template <typename First, typename Second>
auto json_to_native(std::pair<First, Second>& obj, json_to_native_state& state, json_event event, bool start) {
    return false;
}

inline bool json_to_native(std::string& obj, json_to_native_state& state, json_event event, bool start) {
    if (event == json_event::received_string) {
        obj = state.value_string;
        printf("%*sstring: %s\n", int(state.stack.size() * 4), "", obj.c_str());
        return true;
    } else
        return false;
}

} // namespace abieos
