#include <array>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <ctime>
#include <map>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

#include "rapidjson/reader.h"

namespace abieos {

inline constexpr bool trace_json_to_native = false;
inline constexpr bool trace_json_to_native_event = false;

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

template <typename T>
struct root_object_wrapper {
    T& obj;
};

///////////////////////////////////////////////////////////////////////////////
// stream events
///////////////////////////////////////////////////////////////////////////////

enum class event_type {
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

struct event_data {
    bool value_bool = 0;
    uint64_t value_uint64 = 0;
    int64_t value_int64 = 0;
    double value_double = 0;
    std::string value_string{};
    std::string key{};
};

bool receive_event(struct json_to_native_state&, event_type);
bool receive_event(struct json_to_abi_state&, event_type);

template <typename Derived>
struct json_reader_handler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, Derived> {
    event_data received_data;

    Derived& get_derived() { return *static_cast<Derived*>(this); }

    bool Null() { return receive_event(get_derived(), event_type::received_null); }
    bool Bool(bool v) {
        received_data.value_bool = v;
        return receive_event(get_derived(), event_type::received_bool);
    }
    bool Int(int v) { return Int64(v); }
    bool Uint(unsigned v) { return Uint64(v); }
    bool Int64(int64_t v) {
        received_data.value_int64 = v;
        return receive_event(get_derived(), event_type::received_int64);
    }
    bool Uint64(uint64_t v) {
        received_data.value_uint64 = v;
        return receive_event(get_derived(), event_type::received_uint64);
    }
    bool Double(double v) {
        received_data.value_double = v;
        return receive_event(get_derived(), event_type::received_double);
    }
    bool String(const char* v, rapidjson::SizeType length, bool) {
        received_data.value_string = {v, length};
        return receive_event(get_derived(), event_type::received_string);
    }
    bool StartObject() { return receive_event(get_derived(), event_type::received_start_object); }
    bool Key(const char* v, rapidjson::SizeType length, bool) {
        received_data.key = {v, length};
        return receive_event(get_derived(), event_type::received_key);
    }
    bool EndObject(rapidjson::SizeType) { return receive_event(get_derived(), event_type::received_end_object); }
    bool StartArray() { return receive_event(get_derived(), event_type::received_start_array); }
    bool EndArray(rapidjson::SizeType) { return receive_event(get_derived(), event_type::received_end_array); }
};

///////////////////////////////////////////////////////////////////////////////
// state and serializers
///////////////////////////////////////////////////////////////////////////////

struct native_serializer;
struct native_stack_entry {
    void* obj = nullptr;
    const native_serializer* ser = nullptr;
    int position = 0;
};

struct abi_serializer;
struct abi_stack_entry {
    const abi_serializer* ser = nullptr;
    int position = 0;
};

struct json_to_native_state : json_reader_handler<json_to_native_state> {
    std::vector<native_stack_entry> stack;
};

struct bin_to_native_state {
    std::vector<native_stack_entry> stack;
};

struct json_to_abi_state : json_reader_handler<json_to_abi_state> {
    std::vector<abi_stack_entry> stack;
};

struct native_serializer {
    virtual bool bin_to_native(bin_to_native_state&, void*, bool) const = 0;
    virtual bool json_to_native(void*, json_to_native_state&, event_type, bool) const = 0;
};

struct native_field_serializer_methods {
    virtual bool bin_to_native(bin_to_native_state&, void*, bool) const = 0;
    virtual bool json_to_native(void*, json_to_native_state&, event_type, bool) const = 0;
};

struct native_field_serializer {
    std::string_view name = "<unknown>";
    bool required = true;
    const native_field_serializer_methods* methods = nullptr;
};

struct abi_serializer {
    virtual bool json_to_bin(json_to_native_state&, event_type, bool) const = 0;
};

///////////////////////////////////////////////////////////////////////////////
// serializer function prototypes
///////////////////////////////////////////////////////////////////////////////

template <typename T>
bool bin_to_native(T& obj);
template <typename T>
bool bin_to_native(bin_to_native_state& state, T& obj, bool start);
bool bin_to_native(bin_to_native_state& state, std::string& obj, bool start);
template <typename T>
bool bin_to_native(bin_to_native_state& state, root_object_wrapper<T>& wrapper, bool start);

template <typename T>
auto json_to_native(T& obj, json_to_native_state& state, event_type event, bool start)
    -> std::enable_if_t<std::is_arithmetic_v<T>, bool>;
template <typename T>
auto json_to_native(T& obj, json_to_native_state& state, event_type event, bool start)
    -> std::enable_if_t<std::is_class_v<T>, bool>;
template <typename T>
auto json_to_native(std::vector<T>& obj, json_to_native_state& state, event_type event, bool start);
template <typename First, typename Second>
auto json_to_native(std::pair<First, Second>& obj, json_to_native_state& state, event_type event, bool start);
bool json_to_native(std::string& obj, json_to_native_state& state, event_type event, bool start);
template <typename T>
bool json_to_native(root_object_wrapper<T>& obj, json_to_native_state& state, event_type event, bool start);

template <typename T>
auto json_to_bin(T*, json_to_native_state& state, event_type event, bool start)
    -> std::enable_if_t<std::is_arithmetic_v<T>, bool>;
bool json_to_bin(std::string*, json_to_native_state& state, event_type event, bool start);

///////////////////////////////////////////////////////////////////////////////
// serializable types
///////////////////////////////////////////////////////////////////////////////

struct bytes {};

inline bool bin_to_native(bin_to_native_state& state, bytes& obj, bool start) { return true; }
inline bool json_to_native(bytes& obj, json_to_native_state& state, event_type event, bool start) { return false; }
inline bool json_to_bin(bytes*, json_to_native_state& state, event_type event, bool start) { return false; }

inline constexpr uint64_t char_to_symbol(char c) {
    if (c >= 'a' && c <= 'z')
        return (c - 'a') + 6;
    if (c >= '1' && c <= '5')
        return (c - '1') + 1;
    return 0;
}

inline constexpr uint64_t string_to_name(const char* str) {
    uint64_t name = 0;
    int i = 0;
    for (; str[i] && i < 12; ++i)
        name |= (char_to_symbol(str[i]) & 0x1f) << (64 - 5 * (i + 1));
    if (i == 12)
        name |= char_to_symbol(str[12]) & 0x0F;
    return name;
}

inline std::string name_to_string(uint64_t name) {
    static const char* charmap = ".12345abcdefghijklmnopqrstuvwxyz";
    std::string str(13, '.');

    uint64_t tmp = name;
    for (uint32_t i = 0; i <= 12; ++i) {
        char c = charmap[tmp & (i == 0 ? 0x0f : 0x1f)];
        str[12 - i] = c;
        tmp >>= (i == 0 ? 4 : 5);
    }

    const auto last = str.find_last_not_of('.');
    if (last != std::string::npos)
        str = str.substr(0, last + 1);

    return str;
}

struct name {
    uint64_t value = 0;

    constexpr name() = default;
    constexpr explicit name(uint64_t value) : value{value} {}
    constexpr explicit name(const char* str) : value{string_to_name(str)} {}
    constexpr name(const name&) = default;

    explicit operator std::string() const { return name_to_string(value); }
};

inline bool operator<(name a, name b) { return a.value < b.value; }

inline bool bin_to_native(bin_to_native_state& state, name& obj, bool start) {
    return bin_to_native(state, obj.value, start);
}

inline bool json_to_native(name& obj, json_to_native_state& state, event_type event, bool start) {
    if (event == event_type::received_string) {
        obj.value = string_to_name(state.received_data.value_string.c_str());
        if (trace_json_to_native)
            printf("%*sname: %s (%08llx) %s\n", int(state.stack.size() * 4), "",
                   state.received_data.value_string.c_str(), obj.value, std::string{obj}.c_str());
        return true;
    } else
        return false;
}

inline bool json_to_bin(name*, json_to_native_state& state, event_type event, bool start) {
    if (event == event_type::received_string) {
        name obj{string_to_name(state.received_data.value_string.c_str())};
        printf("%*sname: %s (%08llx) %s\n", int(state.stack.size() * 4), "", state.received_data.value_string.c_str(),
               obj.value, std::string{obj}.c_str());
        return true;
    } else
        return false;
}

using action_name = name;
using field_name = std::string;
using table_name = name;
using type_name = std::string;

struct varuint32 {
    uint32_t value = 0;

    varuint32() = default;
    explicit varuint32(uint32_t v) : value(v) {}

    explicit operator uint32_t() { return value; }

    varuint32& operator=(uint32_t v) {
        value = v;
        return *this;
    }
};

inline bool json_to_bin(varuint32*, json_to_native_state& state, event_type event, bool start) {
    varuint32 obj;
    if (event == event_type::received_bool)
        obj = state.received_data.value_bool;
    else if (event == event_type::received_uint64)
        obj = state.received_data.value_uint64;
    else if (event == event_type::received_int64)
        obj = state.received_data.value_int64;
    else if (event == event_type::received_double)
        obj = state.received_data.value_double;
    else
        return false;
    return true;
}

struct time_point_sec {
    uint32_t utc_seconds = 0;

    time_point_sec() = default;

    explicit time_point_sec(uint32_t seconds) : utc_seconds{seconds} {}

    explicit time_point_sec(const std::string& s) {
        static const boost::posix_time::ptime epoch = boost::posix_time::from_time_t(0);
        boost::posix_time::ptime pt;
        if (s.size() >= 5 && s.at(4) == '-') // http://en.wikipedia.org/wiki/ISO_8601
            pt = boost::date_time::parse_delimited_time<boost::posix_time::ptime>(s, 'T');
        else
            pt = boost::posix_time::from_iso_string(s);
        utc_seconds = (pt - epoch).total_seconds();
    }

    explicit operator std::string() {
        const auto ptime = boost::posix_time::from_time_t(time_t(utc_seconds));
        return boost::posix_time::to_iso_extended_string(ptime);
    }
};

inline bool json_to_bin(time_point_sec*, json_to_native_state& state, event_type event, bool start) {
    if (event == event_type::received_string) {
        time_point_sec obj{state.received_data.value_string};
        printf("%*stime_point_sec: %s (%u) %s\n", int(state.stack.size() * 4), "",
               state.received_data.value_string.c_str(), (unsigned)obj.utc_seconds, std::string{obj}.c_str());
        return true;
    } else
        return false;
}

struct asset {};

inline bool json_to_bin(asset*, json_to_native_state& state, event_type event, bool start) {
    if (event == event_type::received_string) {
        return true;
    } else
        return false;
}

///////////////////////////////////////////////////////////////////////////////
// abi types
///////////////////////////////////////////////////////////////////////////////

using extensions_type = std::vector<std::pair<uint16_t, bytes>>;

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

///////////////////////////////////////////////////////////////////////////////
// native serializer implementations
///////////////////////////////////////////////////////////////////////////////

template <typename T>
struct native_serializer_impl : native_serializer {
    bool bin_to_native(bin_to_native_state& state, void* v, bool start) const override {
        return ::abieos::bin_to_native(state, *reinterpret_cast<T*>(v), start);
    }
    bool json_to_native(void* v, json_to_native_state& state, event_type event, bool start) const override {
        return ::abieos::json_to_native(*reinterpret_cast<T*>(v), state, event, start);
    }
};

template <typename T>
inline constexpr auto native_serializer_for = native_serializer_impl<T>{};

template <typename member_ptr>
constexpr auto create_native_field_serializer_methods_impl() {
    struct impl : native_field_serializer_methods {
        bool bin_to_native(bin_to_native_state& state, void* v, bool start) const override {
            return ::abieos::bin_to_native(state, member_from_void(member_ptr{}, v), start);
        }
        bool json_to_native(void* v, json_to_native_state& state, event_type event, bool start) const override {
            return ::abieos::json_to_native(member_from_void(member_ptr{}, v), state, event, start);
        }
    };
    return impl{};
}

template <typename member_ptr>
inline constexpr auto field_serializer_methods_for = create_native_field_serializer_methods_impl<member_ptr>();

template <typename T>
constexpr auto create_native_field_serializers() {
    constexpr auto num_fields = ([&]() constexpr {
        int num_fields = 0;
        for_each_field((T*)nullptr, [&](auto, auto, bool) { ++num_fields; });
        return num_fields;
    }());
    std::array<native_field_serializer, num_fields> fields;
    int i = 0;
    for_each_field((T*)nullptr, [&](auto* name, auto member_ptr, auto required) {
        fields[i++] = {name, required, &field_serializer_methods_for<decltype(member_ptr)>};
    });
    return fields;
}

template <typename T>
inline constexpr auto native_field_serializers_for = create_native_field_serializers<T>();

///////////////////////////////////////////////////////////////////////////////
// bin_to_native
///////////////////////////////////////////////////////////////////////////////

template <typename State, typename T, typename F>
bool serialize_class(State& state, T& obj, bool start, F f) {
    if (start) {
        printf("%*s{ %d fields\n", int(state.stack.size() * 4), "", int(native_field_serializers_for<T>.size()));
        state.stack.push_back({&obj, &native_serializer_for<T>});
        return true;
    }
    auto& stack_entry = state.stack.back();
    if (stack_entry.position < native_field_serializers_for<T>.size()) {
        auto& field_ser = native_field_serializers_for<T>[stack_entry.position];
        printf("%*sfield %d/%d: %s\n", int(state.stack.size() * 4), "", int(stack_entry.position),
               int(native_field_serializers_for<T>.size()),
               std::string{native_field_serializers_for<T>[stack_entry.position].name}.c_str());
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
        state.stack.push_back({&v, &native_serializer_for<std::vector<T>>});
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
        state.stack.push_back({&v, &native_serializer_for<std::pair<First, Second>>});
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
            return native_serializer_for<std::decay_t<decltype(item)>>.bin_to_native(state, &item, true);
        });
    } else if constexpr (is_pair_v<T>) {
        return serialize_pair(state, obj, start, [&](auto& item) { //
            return native_serializer_for<std::decay_t<decltype(item)>>.bin_to_native(state, &item, true);
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

///////////////////////////////////////////////////////////////////////////////
// json_to_native
///////////////////////////////////////////////////////////////////////////////

inline bool receive_event(struct json_to_native_state& state, event_type event) {
    if (state.stack.empty())
        return false;
    auto& x = state.stack.back();
    if (trace_json_to_native_event)
        printf("(event %d)\n", event);
    return x.ser->json_to_native(x.obj, state, event, false);
}

template <typename T>
bool json_to_native(T& obj, std::string_view json) {
    std::string mutable_json{json};
    json_to_native_state state;
    root_object_wrapper<T> wrapper{obj};
    if (!json_to_native(wrapper, state, event_type::received_start_object, true))
        return false;
    rapidjson::Reader reader;
    rapidjson::InsituStringStream ss(mutable_json.data());
    return reader.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag |
                        rapidjson::kParseFullPrecisionFlag>(ss, state);
}

template <typename T>
auto json_to_native(T& obj, json_to_native_state& state, event_type event, bool start)
    -> std::enable_if_t<std::is_arithmetic_v<T>, bool> {

    if (event == event_type::received_bool)
        obj = state.received_data.value_bool;
    else if (event == event_type::received_uint64)
        obj = state.received_data.value_uint64;
    else if (event == event_type::received_int64)
        obj = state.received_data.value_int64;
    else if (event == event_type::received_double)
        obj = state.received_data.value_double;
    else
        return false;
    return true;
}

template <typename T>
bool json_to_native(root_object_wrapper<T>& wrapper, json_to_native_state& state, event_type event, bool start) {
    if (start) {
        state.stack.push_back({&wrapper, &native_serializer_for<root_object_wrapper<T>>});
        return true;
    } else if (event == event_type::received_start_object)
        return json_to_native(wrapper.obj, state, event, true);
    else
        return false;
}

template <typename T>
auto json_to_native(T& obj, json_to_native_state& state, event_type event, bool start)
    -> std::enable_if_t<std::is_class_v<T>, bool> {

    if (start) {
        if (event != event_type::received_start_object)
            return false;
        if (trace_json_to_native)
            printf("%*s{ %d fields\n", int(state.stack.size() * 4), "", int(native_field_serializers_for<T>.size()));
        state.stack.push_back({&obj, &native_serializer_for<T>});
        return true;
    } else if (event == event_type::received_end_object) {
        if (trace_json_to_native)
            printf("%*s}\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return true;
    }
    auto& stack_entry = state.stack.back();
    if (event == event_type::received_key) {
        stack_entry.position = 0;
        while (stack_entry.position < native_field_serializers_for<T>.size() &&
               native_field_serializers_for<T>[stack_entry.position].name != state.received_data.key)
            ++stack_entry.position;
        if (stack_entry.position >= native_field_serializers_for<T>.size())
            return false; // TODO: eat unknown subtree
        return true;
    } else if (stack_entry.position < native_field_serializers_for<T>.size()) {
        auto& field_ser = native_field_serializers_for<T>[stack_entry.position];
        if (trace_json_to_native)
            printf("%*sfield %d/%d: %s (event %d)\n", int(state.stack.size() * 4), "", int(stack_entry.position),
                   int(native_field_serializers_for<T>.size()), std::string{field_ser.name}.c_str(), event);
        return field_ser.methods->json_to_native(&obj, state, event, true);
    } else {
        return true;
    }
    return true;
}

template <typename T>
auto json_to_native(std::vector<T>& v, json_to_native_state& state, event_type event, bool start) {
    if (start) {
        if (event != event_type::received_start_array)
            return false;
        if (trace_json_to_native)
            printf("%*s[\n", int(state.stack.size() * 4), "");
        state.stack.push_back({&v, &native_serializer_for<std::vector<T>>});
        return true;
    } else if (event == event_type::received_end_array) {
        if (trace_json_to_native)
            printf("%*s]\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return true;
    }
    if (trace_json_to_native)
        printf("%*sitem %d (event %d)\n", int(state.stack.size() * 4), "", int(v.size()), event);
    v.emplace_back();
    return json_to_native(v.back(), state, event, true);
}

template <typename First, typename Second>
auto json_to_native(std::pair<First, Second>& obj, json_to_native_state& state, event_type event, bool start) {
    return false; // TODO
}

inline bool json_to_native(std::string& obj, json_to_native_state& state, event_type event, bool start) {
    if (event == event_type::received_string) {
        obj = state.received_data.value_string;
        if (trace_json_to_native)
            printf("%*sstring: %s\n", int(state.stack.size() * 4), "", obj.c_str());
        return true;
    } else
        return false;
}

///////////////////////////////////////////////////////////////////////////////
// abi serializer implementations
///////////////////////////////////////////////////////////////////////////////

template <typename F>
constexpr void for_each_abi_type(F f) {
    static_assert(sizeof(float) == 4);
    static_assert(sizeof(double) == 8);

    f("bool", (bool*)nullptr);
    f("int8", (int8_t*)nullptr);
    f("uint8", (uint8_t*)nullptr);
    f("int16", (int16_t*)nullptr);
    f("uint16", (uint16_t*)nullptr);
    f("int32", (int32_t*)nullptr);
    f("uint32", (uint32_t*)nullptr);
    f("int64", (int64_t*)nullptr);
    f("uint64", (uint64_t*)nullptr);
    // f("int128", (int128_t*)nullptr);
    // f("uint128", (uint128_t*)nullptr);
    // f("varint32", (signed_int*)nullptr);
    f("varuint32", (varuint32*)nullptr);
    f("float32", (float*)nullptr);
    f("float64", (double*)nullptr);
    // f("float128", (uint128_t*)nullptr);
    // f("time_point", (time_point*)nullptr);
    f("time_point_sec", (time_point_sec*)nullptr);
    // f("block_timestamp_type", (block_timestamp_type*)nullptr);
    f("name", (name*)nullptr);
    f("bytes", (bytes*)nullptr);
    f("string", (std::string*)nullptr);
    // f("checksum160", (checksum160_type*)nullptr);
    // f("checksum256", (checksum256_type*)nullptr);
    // f("checksum512", (checksum512_type*)nullptr);
    // f("public_key", (public_key_type*)nullptr);
    // f("signature", (signature_type*)nullptr);
    // f("symbol", (symbol*)nullptr);
    // f("symbol_code", (symbol_code*)nullptr);
    f("asset", (asset*)nullptr);
    // f("extended_asset", (extended_asset*)nullptr);
}

template <typename T>
struct abi_serializer_impl : abi_serializer {
    bool json_to_bin(json_to_native_state& state, event_type event, bool start) const override {
        return ::abieos::json_to_bin((T*)nullptr, state, event, start);
    }
};

template <typename T>
inline constexpr auto abi_serializer_for = abi_serializer_impl<T>{};

///////////////////////////////////////////////////////////////////////////////
// abi handling
///////////////////////////////////////////////////////////////////////////////

struct abi_field {
    field_name name{};
    struct abi_type* type{};
};

struct abi_type {
    type_name name{};
    type_name alias_of_name{};
    const struct_def* struct_def{};
    abi_type* alias_of{};
    abi_type* array_of{};
    abi_type* base{};
    std::vector<abi_field> fields{};
    bool filled_struct{};
    const abi_serializer* ser{};
};

struct contract {
    std::map<type_name, abi_type> abi_types;
};

template <int i>
bool ends_with(const std::string& s, const char (&suffix)[i]) {
    return s.size() >= i - 1 && !strcmp(s.c_str() + s.size() - (i - 1), suffix);
}

inline abi_type& get_type(std::map<type_name, abi_type>& abi_types, const type_name& name, int depth) {
    if (depth >= 32)
        throw std::runtime_error("abi recursion limit reached");
    auto it = abi_types.find(name);
    if (it == abi_types.end()) {
        // todo: optional
        if (ends_with(name, "[]")) {
            abi_type type{name};
            type.array_of = &get_type(abi_types, name.substr(0, name.size() - 2), depth + 1);
            return abi_types[name] = std::move(type);
        } else
            throw std::runtime_error("abi references unknown type \"" + name + "\"");
    }
    if (it->second.alias_of)
        return *it->second.alias_of;
    if (it->second.alias_of_name.empty())
        return it->second;
    auto& other = get_type(abi_types, it->second.alias_of_name, depth + 1);
    it->second.alias_of = &other;
    return other;
}

inline abi_type& fill_struct(std::map<type_name, abi_type>& abi_types, abi_type& type, int depth) {
    if (depth >= 32)
        throw std::runtime_error("abi recursion limit reached");
    if (type.filled_struct)
        return type;
    if (!type.struct_def)
        throw std::runtime_error("abi type \"" + type.name + "\" is not a struct");
    if (!type.struct_def->base.empty())
        type.fields = fill_struct(abi_types, get_type(abi_types, type.struct_def->base, depth + 1), depth + 1).fields;
    for (auto& field : type.struct_def->fields)
        type.fields.push_back(abi_field{field.name, &get_type(abi_types, field.type, depth + 1)});
    return type;
}

inline contract create_contract(const abi_def& abi) {
    contract c;
    for_each_abi_type([&](const char* name, auto* p) {
        abi_type type{name};
        type.ser = &abi_serializer_for<std::decay_t<decltype(*p)>>;
        c.abi_types.insert({type.name, std::move(type)});
    });
    for (auto& t : abi.types) {
        if (t.new_type_name.empty())
            throw std::runtime_error("abi has a type with a missing name");
        auto [_, inserted] = c.abi_types.insert({t.new_type_name, abi_type{t.new_type_name, t.type}});
        if (!inserted)
            throw std::runtime_error("abi redefines type \"" + t.new_type_name + "\"");
    }
    for (auto& s : abi.structs) {
        if (s.name.empty())
            throw std::runtime_error("abi has a struct with a missing name");
        auto [_, inserted] = c.abi_types.insert({s.name, abi_type{s.name, {}, &s}});
        if (!inserted)
            throw std::runtime_error("abi redefines type \"" + s.name + "\"");
    }
    for (auto& [_, t] : c.abi_types)
        if (!t.alias_of_name.empty())
            t.alias_of = &get_type(c.abi_types, t.alias_of_name, 0);
    for (auto& [_, t] : c.abi_types)
        if (t.struct_def)
            fill_struct(c.abi_types, t, 0);
    return c;
}

///////////////////////////////////////////////////////////////////////////////
// json_to_bin
///////////////////////////////////////////////////////////////////////////////

template <typename T>
auto json_to_bin(T*, json_to_native_state& state, event_type event, bool start)
    -> std::enable_if_t<std::is_arithmetic_v<T>, bool> {
    T obj;
    if (event == event_type::received_bool)
        obj = state.received_data.value_bool;
    else if (event == event_type::received_uint64)
        obj = state.received_data.value_uint64;
    else if (event == event_type::received_int64)
        obj = state.received_data.value_int64;
    else if (event == event_type::received_double)
        obj = state.received_data.value_double;
    else
        return false;
    return true;
}

inline bool json_to_bin(std::string*, json_to_native_state& state, event_type event, bool start) {
    if (event == event_type::received_string) {
        printf("%*sstring: %s\n", int(state.stack.size() * 4), "", state.received_data.value_string.c_str());
        return true;
    } else
        return false;
}

} // namespace abieos
