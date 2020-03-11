// copyright defined in abieos/LICENSE.txt

#pragma once

#include <eosio/chain_conversions.hpp>
#include <eosio/from_bin.hpp>
#include <eosio/from_json.hpp>
#include <eosio/reflection.hpp>
#include <eosio/to_bin.hpp>
#include <eosio/to_json.hpp>
#include <eosio/abi.hpp>
#include <eosio/operators.hpp>
#include <eosio/bytes.hpp>
#include <eosio/crypto.hpp>
#include <eosio/symbol.hpp>
#include <eosio/asset.hpp>
#include <eosio/time.hpp>
#include <eosio/fixed_bytes.hpp>
#include <eosio/float.hpp>
#include <eosio/varint.hpp>

#ifdef __eosio_cdt__
#include <cwchar>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-W#warnings"
#endif

#include <ctime>
#include <map>
#include <optional>
#include <variant>
#include <vector>

#ifdef __eosio_cdt__
#pragma clang diagnostic pop
#endif

#include "abieos_numeric.hpp"

#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace abieos {

using eosio::from_bin;
using eosio::to_bin;

inline constexpr bool trace_json_to_jvalue_event = false;
inline constexpr bool trace_json_to_jvalue = false;
inline constexpr bool trace_jvalue_to_bin = false;
inline constexpr bool trace_json_to_bin = false;
inline constexpr bool trace_json_to_bin_event = false;
inline constexpr bool trace_bin_to_json = false;

inline constexpr size_t max_stack_size = 128;

// Pseudo objects never exist, except in serialized form
struct pseudo_optional;
struct pseudo_extension;
struct pseudo_object;
struct pseudo_array;
struct pseudo_variant;

// !!!
template <typename SrcIt, typename DestIt>
void hex(SrcIt begin, SrcIt end, DestIt dest) {
    auto nibble = [&dest](uint8_t i) {
        if (i <= 9)
            *dest++ = '0' + i;
        else
            *dest++ = 'A' + i - 10;
    };
    while (begin != end) {
        nibble(((uint8_t)*begin) >> 4);
        nibble(((uint8_t)*begin) & 0xf);
        ++begin;
    }
}

// !!!
template <typename SrcIt>
std::string hex(SrcIt begin, SrcIt end) {
    std::string s;
    hex(begin, end, std::back_inserter(s));
    return s;
}

// !!!
template <typename SrcIt, typename DestIt>
ABIEOS_NODISCARD bool unhex(std::string& error, SrcIt begin, SrcIt end, DestIt dest) {
    auto get_digit = [&](uint8_t& nibble) {
        if (*begin >= '0' && *begin <= '9')
            nibble = *begin++ - '0';
        else if (*begin >= 'a' && *begin <= 'f')
            nibble = *begin++ - 'a' + 10;
        else if (*begin >= 'A' && *begin <= 'F')
            nibble = *begin++ - 'A' + 10;
        else
            return set_error(error, "expected hex string");
        return true;
    };
    while (begin != end) {
        uint8_t h, l;
        if (!get_digit(h) || !get_digit(l))
            return false;
        *dest++ = (h << 4) | l;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// stream events
///////////////////////////////////////////////////////////////////////////////

enum class event_type {
    received_null,         // 0
    received_bool,         // 1
    received_string,       // 2
    received_start_object, // 3
    received_key,          // 4
    received_end_object,   // 5
    received_start_array,  // 6
    received_end_array,    // 7
};

struct event_data {
    bool value_bool = 0;
    std::string value_string{};
    std::string key{};
};

template <typename Derived>
struct json_reader_handler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, Derived> {
    event_data received_data{};
    bool started = false;

    Derived& get_derived() { return *static_cast<Derived*>(this); }

    bool get_start() {
        if (started)
            return false;
        started = true;
        return true;
    }

    bool get_bool() const { return received_data.value_bool; }

    const std::string& get_string() const { return received_data.value_string; }

    bool Null() { return receive_event(get_derived(), event_type::received_null, get_start()); }
    bool Bool(bool v) {
        received_data.value_bool = v;
        return receive_event(get_derived(), event_type::received_bool, get_start());
    }
    bool RawNumber(const char* v, rapidjson::SizeType length, bool copy) { return String(v, length, copy); }
    bool Int(int v) { return false; }
    bool Uint(unsigned v) { return false; }
    bool Int64(int64_t v) { return false; }
    bool Uint64(uint64_t v) { return false; }
    bool Double(double v) { return false; }
    bool String(const char* v, rapidjson::SizeType length, bool) {
        received_data.value_string = {v, length};
        return receive_event(get_derived(), event_type::received_string, get_start());
    }
    bool StartObject() { return receive_event(get_derived(), event_type::received_start_object, get_start()); }
    bool Key(const char* v, rapidjson::SizeType length, bool) {
        received_data.key = {v, length};
        return receive_event(get_derived(), event_type::received_key, get_start());
    }
    bool EndObject(rapidjson::SizeType) {
        return receive_event(get_derived(), event_type::received_end_object, get_start());
    }
    bool StartArray() { return receive_event(get_derived(), event_type::received_start_array, get_start()); }
    bool EndArray(rapidjson::SizeType) {
        return receive_event(get_derived(), event_type::received_end_array, get_start());
    }
};

///////////////////////////////////////////////////////////////////////////////
// json model
///////////////////////////////////////////////////////////////////////////////

struct jvalue;
using jarray = std::vector<jvalue>;
using jobject = std::map<std::string, jvalue>;

struct jvalue {
    std::variant<std::nullptr_t, bool, std::string, jobject, jarray> value;
};

///////////////////////////////////////////////////////////////////////////////
// state and serializers
///////////////////////////////////////////////////////////////////////////////

using eosio::abi_type;

struct size_insertion {
    size_t position = 0;
    uint32_t size = 0;
};

struct json_to_jvalue_stack_entry {
    jvalue* value = nullptr;
    std::string key = "";
};

struct jvalue_to_bin_stack_entry {
    const abi_type* type = nullptr;
    bool allow_extensions = false;
    const jvalue* value = nullptr;
    int position = -1;
};

struct json_to_bin_stack_entry {
    const abi_type* type = nullptr;
    bool allow_extensions = false;
    int position = -1;
    size_t size_insertion_index = 0;
    size_t variant_type_index = 0;
};

struct bin_to_json_stack_entry {
    const abi_type* type = nullptr;
    bool allow_extensions = false;
    int position = -1;
    uint32_t array_size = 0;
};

struct json_to_jvalue_state : json_reader_handler<json_to_jvalue_state> {
    std::string& error;
    std::vector<json_to_jvalue_stack_entry> stack;

    json_to_jvalue_state(std::string& error) : error{error} {}
};

struct jvalue_to_bin_state {
    eosio::vector_stream writer;
    const jvalue* received_value = nullptr;
    std::vector<jvalue_to_bin_stack_entry> stack{};
    bool skipped_extension = false;

    eosio::result<bool> get_bool() const {
        if(auto* b = std::get_if<bool>(&received_value->value)) {
            return *b;
        } else {
            return eosio::from_json_error::expected_bool;
        }
    }

    eosio::result<std::string_view> get_string() const {
        if(auto* s = std::get_if<std::string>(&received_value->value)) {
            return *s;
        } else {
            return eosio::from_json_error::expected_string;
        }
    }
    eosio::result<void> get_null() {
        if(std::holds_alternative<std::nullptr_t>(received_value->value))
            return eosio::outcome::success();
        return eosio::from_json_error::expected_null;
    }
};

struct json_to_bin_state : eosio::json_token_stream {
    using json_token_stream::json_token_stream;
    eosio::result<void> error{eosio::outcome::success()};
    eosio::vector_stream& writer;
    std::vector<size_insertion> size_insertions{};
    std::vector<json_to_bin_stack_entry> stack{};
    bool skipped_extension = false;

    explicit json_to_bin_state(char* in, eosio::vector_stream& out)
      : eosio::json_token_stream(in), writer(out) {}
};

struct bin_to_json_state {
    eosio::input_stream& bin;
    eosio::vector_stream& writer;
    std::vector<bin_to_json_stack_entry> stack{};
    bool skipped_extension = false;

    bin_to_json_state(eosio::input_stream& bin, eosio::vector_stream& writer)
        : bin{bin}, writer{writer} {}
};

}

namespace eosio {

struct abi_serializer {
  virtual eosio::result<void> json_to_bin(::abieos::jvalue_to_bin_state& state, bool allow_extensions, const abi_type* type,
                                          bool start) const = 0;
  virtual eosio::result<void> json_to_bin(::abieos::json_to_bin_state& state, bool allow_extensions, const abi_type* type,
                                          bool start) const = 0;
  virtual eosio::result<void> bin_to_json(::abieos::bin_to_json_state& state, bool allow_extensions, const abi_type* type,
                                          bool start) const = 0;
};

}

namespace abieos {

///////////////////////////////////////////////////////////////////////////////
// serializer function prototypes
///////////////////////////////////////////////////////////////////////////////

template <typename State>
eosio::result<void> json_to_bin(pseudo_optional*, State& state, bool allow_extensions,
                                const abi_type* type, bool);
template <typename State>
eosio::result<void> json_to_bin(pseudo_extension*, State& state, bool allow_extensions,
                                const abi_type* type, bool);
template <typename T, typename State>
eosio::result<void> json_to_bin(T*, State& state, bool allow_extensions, const abi_type*,
                                bool start);
template <typename State>
eosio::result<void> json_to_bin(std::string*, jvalue_to_bin_state& state, bool allow_extensions, const abi_type*,
                                bool start);

eosio::result<void> json_to_bin(pseudo_object*, jvalue_to_bin_state& state, bool allow_extensions,
                                const abi_type* type, bool start);
eosio::result<void> json_to_bin(pseudo_array*, jvalue_to_bin_state& state, bool allow_extensions,
                                const abi_type* type, bool start);
eosio::result<void> json_to_bin(pseudo_variant*, jvalue_to_bin_state& state, bool allow_extensions,
                                const abi_type* type, bool start);

eosio::result<void> json_to_bin(pseudo_object*, json_to_bin_state& state, bool allow_extensions, const abi_type* type,
                                bool start);
eosio::result<void> json_to_bin(pseudo_array*, json_to_bin_state& state, bool allow_extensions, const abi_type* type,
                                bool start);
eosio::result<void> json_to_bin(pseudo_variant*, json_to_bin_state& state, bool allow_extensions,
                                const abi_type* type, bool start);

eosio::result<void> bin_to_json(pseudo_optional*, bin_to_json_state& state, bool allow_extensions,
                                const abi_type* type, bool start);
eosio::result<void> bin_to_json(pseudo_extension*, bin_to_json_state& state, bool allow_extensions,
                                const abi_type* type, bool start);
eosio::result<void> bin_to_json(pseudo_object*, bin_to_json_state& state, bool allow_extensions, const abi_type* type,
                                bool start);
eosio::result<void> bin_to_json(pseudo_array*, bin_to_json_state& state, bool allow_extensions, const abi_type* type,
                                bool start);
eosio::result<void> bin_to_json(pseudo_variant*, bin_to_json_state& state, bool allow_extensions,
                                const abi_type* type, bool start);

///////////////////////////////////////////////////////////////////////////////
// serializable types
///////////////////////////////////////////////////////////////////////////////

using eosio::bytes;

template <typename State>
eosio::result<void> json_to_bin(bytes*, State& state, bool, const abi_type*, bool start) {
    OUTCOME_TRY(s, state.get_string());;
    if (trace_json_to_bin)
        printf("%*sbytes (%d hex digits)\n", int(state.stack.size() * 4), "", int(s.size()));
    if (s.size() & 1)
        return eosio::from_json_error::expected_hex_string;
    OUTCOME_TRY(eosio::varuint32_to_bin(s.size() / 2, state.writer));
    // FIXME: Add a function to encode a hex string to a stream
    if(!eosio::unhex(std::back_inserter(state.writer.data), s.begin(), s.end()))
        return eosio::from_json_error::expected_hex_string;
    return eosio::outcome::success();
}

inline eosio::result<void> bin_to_json(bytes*, bin_to_json_state& state, bool, const abi_type*, bool start) {
    uint64_t size;
    OUTCOME_TRY(varuint64_from_bin(size, state.bin));
    const char* data;
    OUTCOME_TRY(state.bin.read_reuse_storage(data, size));
    return to_json_hex(data, size, state.writer);
}

using eosio::float128;
using eosio::checksum160;
using eosio::checksum256;
using eosio::checksum512;

using uint128 = unsigned __int128;
using int128 = __int128;

using eosio::public_key;
using eosio::private_key;
using eosio::signature;
using eosio::name;

using eosio::varuint32;
using eosio::varint32;

using eosio::time_point;
using eosio::time_point_sec;
using eosio::block_timestamp;
using eosio::symbol_code;
using eosio::symbol;
using eosio::asset;
using eosio::extended_asset;

///////////////////////////////////////////////////////////////////////////////
// abi types
///////////////////////////////////////////////////////////////////////////////

using extensions_type = std::vector<std::pair<uint16_t, bytes>>;

using eosio::abi_def;

ABIEOS_NODISCARD inline bool check_abi_version(const std::string& s, std::string& error) {
    if (s.substr(0, 13) != "eosio::abi/1.")
        return set_error(error, "unsupported abi version");
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// json_to_jvalue
///////////////////////////////////////////////////////////////////////////////

ABIEOS_NODISCARD bool json_to_jobject(jvalue& value, json_to_jvalue_state& state, event_type event, bool start);
ABIEOS_NODISCARD bool json_to_jarray(jvalue& value, json_to_jvalue_state& state, event_type event, bool start);

ABIEOS_NODISCARD inline bool receive_event(struct json_to_jvalue_state& state, event_type event, bool start) {
    if (state.stack.empty())
        return set_error(state, "extra data");
    if (state.stack.size() > max_stack_size)
        return set_error(state, "recursion limit reached");
    if (trace_json_to_jvalue_event)
        printf("(event %d)\n", (int)event);
    auto& v = *state.stack.back().value;
    if (start) {
        state.stack.pop_back();
        if (event == event_type::received_null) {
            v.value = nullptr;
        } else if (event == event_type::received_bool) {
            v.value = state.get_bool();
        } else if (event == event_type::received_string) {
            v.value = std::move(state.get_string());
        } else if (event == event_type::received_start_object) {
            v.value = jobject{};
            return json_to_jobject(v, state, event, start);
        } else if (event == event_type::received_start_array) {
            v.value = jarray{};
            return json_to_jarray(v, state, event, start);
        } else {
            return false;
        }
    } else {
        if (std::holds_alternative<jobject>(v.value))
            return json_to_jobject(v, state, event, start);
        else if (std::holds_alternative<jarray>(v.value))
            return json_to_jarray(v, state, event, start);
        else
            return set_error(state, "extra data");
    }
    return true;
}

template<typename F>
ABIEOS_NODISCARD inline eosio::result<void> json_to_jvalue(jvalue& value, std::string_view json, F&& f) {
    std::string mutable_json{json};
    mutable_json.push_back(0);
    mutable_json.push_back(0);
    mutable_json.push_back(0);
    std::string error; // !!!
    json_to_jvalue_state state{error};
    state.stack.push_back({&value});
    rapidjson::Reader reader;
    rapidjson::InsituStringStream ss(mutable_json.data());
    if (reader.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag |
        rapidjson::kParseNumbersAsStringsFlag>(ss, state))
        return eosio::outcome::success();
    else
        return eosio::from_json_error::unspecific_syntax_error; // !!!
}

ABIEOS_NODISCARD inline bool json_to_jobject(jvalue& value, json_to_jvalue_state& state, event_type event, bool start) {
    if (start) {
        if (event != event_type::received_start_object)
            return set_error(state, "expected object");
        if (trace_json_to_jvalue)
            printf("%*s{\n", int(state.stack.size() * 4), "");
        state.stack.push_back({&value});
        return true;
    } else if (event == event_type::received_end_object) {
        if (trace_json_to_jvalue)
            printf("%*s}\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return true;
    }
    auto& stack_entry = state.stack.back();
    if (event == event_type::received_key) {
        stack_entry.key = std::move(state.received_data.key);
        return true;
    } else {
        if (trace_json_to_jvalue)
            printf("%*sfield %s (event %d)\n", int(state.stack.size() * 4), "", stack_entry.key.c_str(), (int)event);
        auto& x = std::get<jobject>(value.value)[stack_entry.key] = {};
        state.stack.push_back({&x});
        return receive_event(state, event, true);
    }
}

ABIEOS_NODISCARD inline bool json_to_jarray(jvalue& value, json_to_jvalue_state& state, event_type event, bool start) {
    if (start) {
        if (event != event_type::received_start_array)
            return set_error(state, "expected array");
        if (trace_json_to_jvalue)
            printf("%*s[\n", int(state.stack.size() * 4), "");
        state.stack.push_back({&value});
        return true;
    } else if (event == event_type::received_end_array) {
        if (trace_json_to_jvalue)
            printf("%*s]\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return true;
    }
    auto& v = std::get<jarray>(value.value);
    if (trace_json_to_jvalue)
        printf("%*sitem %d (event %d)\n", int(state.stack.size() * 4), "", int(v.size()), (int)event);
    v.emplace_back();
    state.stack.push_back({&v.back()});
    return receive_event(state, event, true);
}

///////////////////////////////////////////////////////////////////////////////
// abi serializer implementations
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// abi handling
///////////////////////////////////////////////////////////////////////////////

using abi = eosio::abi;

///////////////////////////////////////////////////////////////////////////////
// json_to_bin (jvalue)
///////////////////////////////////////////////////////////////////////////////

template<typename F>
inline eosio::result<void> json_to_bin(std::vector<char>& bin, const abi_type* type, const jvalue& value, F&& f) {
    jvalue_to_bin_state state{{bin}, &value};
    OUTCOME_TRY(type->ser->json_to_bin(state, true, type, true));
    while (!state.stack.empty()) {
        f();
        auto& entry = state.stack.back();
        OUTCOME_TRY(entry.type->ser->json_to_bin(state, entry.allow_extensions, entry.type, false));
    }
    return eosio::outcome::success();
}

template<typename State>
inline eosio::result<void> json_to_bin(pseudo_optional*, State& state, bool allow_extensions,
                                       const abi_type* type, bool) {
    if (state.get_null()) {
        return state.writer.write(char(0));
    }
    OUTCOME_TRY(state.writer.write(char(1)));
    const abi_type* t = type->optional_of();
    return t->ser->json_to_bin(state, allow_extensions, t, true);
}

template<typename State>
inline eosio::result<void> json_to_bin(pseudo_extension*, State& state, bool allow_extensions,
                                       const abi_type* type, bool) {
    const abi_type* t = type->extension_of();
    return t->ser->json_to_bin(state, allow_extensions, t, true);
}

inline eosio::result<void> json_to_bin(pseudo_object*, jvalue_to_bin_state& state, bool allow_extensions,
                                       const abi_type* type, bool start) {
    if (start) {
        if (!state.received_value || !std::holds_alternative<jobject>(state.received_value->value))
            return eosio::from_json_error::expected_start_object;
        if (trace_jvalue_to_bin)
            printf("%*s{ %d fields, allow_ex=%d\n", int(state.stack.size() * 4), "", int(type->as_struct()->fields.size()),
                   allow_extensions);
        state.stack.push_back({type, allow_extensions, state.received_value, -1});
        return eosio::outcome::success();
    }
    auto& stack_entry = state.stack.back();
    ++stack_entry.position;
    const std::vector<eosio::abi_field>& fields = stack_entry.type->as_struct()->fields;
    if (stack_entry.position == (int)fields.size()) {
        if (trace_jvalue_to_bin)
            printf("%*s}\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return eosio::outcome::success();
    }
    auto& field = fields[stack_entry.position];
    auto& obj = std::get<jobject>(stack_entry.value->value);
    auto it = obj.find(field.name);
    if (trace_jvalue_to_bin)
        printf("%*sfield %d/%d: %s\n", int(state.stack.size() * 4), "", int(stack_entry.position),
               int(fields.size()), std::string{field.name}.c_str());
    if (it == obj.end()) {
        if (field.type->extension_of() && allow_extensions) {
            state.skipped_extension = true;
            return eosio::outcome::success();
        }
        stack_entry.position = -1;
        return eosio::from_json_error::expected_field;
    }
    if (state.skipped_extension)
        return eosio::from_json_error::unexpected_field;
    state.received_value = &it->second;
    return field.type->ser->json_to_bin(state, allow_extensions && &field == &fields.back(),
                                        field.type, true);
}

inline eosio::result<void> json_to_bin(pseudo_array*, jvalue_to_bin_state& state, bool, const abi_type* type,
                                       bool start) {
    if (start) {
        if (!state.received_value || !std::holds_alternative<jarray>(state.received_value->value))
            return eosio::from_json_error::expected_start_array;
        if (trace_jvalue_to_bin)
            printf("%*s[ %d elements\n", int(state.stack.size() * 4), "",
                   int(std::get<jarray>(state.received_value->value).size()));
        OUTCOME_TRY(eosio::varuint32_to_bin(std::get<jarray>(state.received_value->value).size(), state.writer));
        state.stack.push_back({type, false, state.received_value, -1});
        return eosio::outcome::success();
    }
    auto& stack_entry = state.stack.back();
    auto& arr = std::get<jarray>(stack_entry.value->value);
    ++stack_entry.position;
    if (stack_entry.position == (int)arr.size()) {
        if (trace_jvalue_to_bin)
            printf("%*s]\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return eosio::outcome::success();
    }
    state.received_value = &arr[stack_entry.position];
    if (trace_jvalue_to_bin)
        printf("%*sitem\n", int(state.stack.size() * 4), "");
    const abi_type * t = type->array_of();
    return t->ser->json_to_bin(state, false, t, true);
}

inline eosio::result<void> json_to_bin(pseudo_variant*, jvalue_to_bin_state& state, bool allow_extensions,
                                       const abi_type* type, bool start) {
    if (start) {
        if (!state.received_value || !std::holds_alternative<jarray>(state.received_value->value))
            return eosio::from_json_error::expected_variant;
        auto& arr = std::get<jarray>(state.received_value->value);
        if (arr.size() != 2)
            return eosio::from_json_error::expected_variant;
        if (!std::holds_alternative<std::string>(arr[0].value))
            return eosio::from_json_error::expected_variant;
        auto& typeName = std::get<std::string>(arr[0].value);
        if (trace_jvalue_to_bin)
            printf("%*s[ variant %s\n", int(state.stack.size() * 4), "", typeName.c_str());
        state.stack.push_back({type, allow_extensions, state.received_value, 0});
        return eosio::outcome::success();
    }
    auto& stack_entry = state.stack.back();
    auto& arr = std::get<jarray>(stack_entry.value->value);
    if (stack_entry.position == 0) {
        auto& typeName = std::get<std::string>(arr[0].value);
        const std::vector<eosio::abi_field>& fields = *stack_entry.type->as_variant();
        auto it = std::find_if(fields.begin(), fields.end(),
                               [&](auto& field) { return field.name == typeName; });
        if (it == fields.end())
            return eosio::from_json_error::invalid_type_for_variant;
        OUTCOME_TRY(eosio::varuint32_to_bin(it - fields.begin(), state.writer));
        state.received_value = &arr[++stack_entry.position];
        return it->type->ser->json_to_bin(state, allow_extensions, it->type, true);
    } else {
        if (trace_jvalue_to_bin)
            printf("%*s]\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return eosio::outcome::success();
    }
}

template <typename T, typename State>
eosio::result<void> json_to_bin(T*, State& state, bool, const abi_type*, bool start) {
    using eosio::from_json;
    T x;
    OUTCOME_TRY(from_json(x, state));
    return to_bin(x, state.writer);
}

template <typename State>
inline eosio::result<void> json_to_bin(std::string*, State& state, bool, const abi_type*,
                                       bool start) {
    OUTCOME_TRY(s, state.get_string());
    if (trace_jvalue_to_bin)
        printf("%*sstring: %.*s\n", int(state.stack.size() * 4), "", (int)s.size(), s.data());
    return to_bin(s, state.writer);
}

///////////////////////////////////////////////////////////////////////////////
// json_to_bin
///////////////////////////////////////////////////////////////////////////////

template<typename F>
inline eosio::result<void> json_to_bin(std::vector<char>& bin, const abi_type* type, std::string_view json, F&& f) {
    std::string mutable_json{json};
    mutable_json.push_back(0);
    mutable_json.push_back(0);
    mutable_json.push_back(0);
    std::vector<char> out_buf;
    eosio::vector_stream out(out_buf);
    json_to_bin_state state(mutable_json.data(), out);

    OUTCOME_TRY(type->ser->json_to_bin(state, true, type, true));
    while(!state.stack.empty()) {
        f();
        auto entry = state.stack.back();
        auto* type = entry.type;
        if (state.stack.size() > max_stack_size)
            return eosio::abi_error::recursion_limit_reached;
        OUTCOME_TRY(type->ser->json_to_bin(state, entry.allow_extensions, type, false));
    }
    if(!state.complete())
        return eosio::from_json_error::expected_end;

    size_t pos = 0;
    for (auto& insertion : state.size_insertions) {
        bin.insert(bin.end(), out_buf.begin() + pos, out_buf.begin() + insertion.position);
        eosio::push_varuint32(bin, insertion.size);
        pos = insertion.position;
    }
    bin.insert(bin.end(), out_buf.begin() + pos, out_buf.end());
    return eosio::outcome::success();
}

inline eosio::result<void> json_to_bin(pseudo_object*, json_to_bin_state& state, bool allow_extensions,
                                       const abi_type* type, bool start) {
    if (start) {
        OUTCOME_TRY(state.get_start_object());
        if (trace_json_to_bin)
            printf("%*s{ %d fields, allow_ex=%d\n", int(state.stack.size() * 4), "", int(type->as_struct()->fields.size()),
                   allow_extensions);
        state.stack.push_back({type, allow_extensions});
        return eosio::outcome::success();
    }
    auto& stack_entry = state.stack.back();
    const std::vector<eosio::abi_field>& fields = type->as_struct()->fields;
    if (state.get_end_object()) {
        if (stack_entry.position + 1 != (ptrdiff_t)fields.size()) {
            auto& field = fields[stack_entry.position + 1];
            if (!field.type->extension_of() || !allow_extensions) {
                stack_entry.position = -1;
                return eosio::from_json_error::expected_field;
            }
            ++stack_entry.position;
            state.skipped_extension = true;
        }
        if (trace_json_to_bin)
            printf("%*s}\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return eosio::outcome::success();
    }
    if (auto key = state.get_key()) {
        if (++stack_entry.position >= (ptrdiff_t)fields.size() || state.skipped_extension)
            return eosio::from_json_error::unexpected_field;
        auto& field = fields[stack_entry.position];
        if (key.value() != field.name) {
            stack_entry.position = -1;
            return eosio::from_json_error::expected_field;
        }
        return eosio::outcome::success();
    } else {
        auto& field = fields[stack_entry.position];
        if (trace_json_to_bin)
            printf("%*sfield %d/%d: %s\n", int(state.stack.size() * 4), "", int(stack_entry.position),
                   int(fields.size()), std::string{field.name}.c_str());
        return field.type->ser->json_to_bin(state, allow_extensions && &field == &fields.back(), field.type,
                                            true);
    }
}

inline eosio::result<void> json_to_bin(pseudo_array*, json_to_bin_state& state, bool, const abi_type* type,
                                       bool start) {
    if (start) {
        OUTCOME_TRY(state.get_start_array());
        if (trace_json_to_bin)
            printf("%*s[\n", int(state.stack.size() * 4), "");
        state.stack.push_back({type, false});
        state.stack.back().size_insertion_index = state.size_insertions.size();
        // FIXME: add Stream::tellp or similar.
        state.size_insertions.push_back({state.writer.data.size()});
        return eosio::outcome::success();
    }
    auto& stack_entry = state.stack.back();
    if (state.get_end_array()) {
        if (trace_json_to_bin)
            printf("%*s]\n", int((state.stack.size() - 1) * 4), "");
        state.size_insertions[stack_entry.size_insertion_index].size = stack_entry.position + 1;
        state.stack.pop_back();
        return eosio::outcome::success();
    }
    ++stack_entry.position;
    if (trace_json_to_bin)
        printf("%*sitem\n", int(state.stack.size() * 4), "");
    const abi_type* t = type->array_of();
    return t->ser->json_to_bin(state, false, t, true);
}

inline eosio::result<void> json_to_bin(pseudo_variant*, json_to_bin_state& state, bool allow_extensions,
                                       const abi_type* type, bool start) {
    if (start) {
        OUTCOME_TRY(state.get_start_array());
        if (trace_json_to_bin)
            printf("%*s[ variant\n", int(state.stack.size() * 4), "");
        state.stack.push_back({type, allow_extensions});
        return eosio::outcome::success();
    }
    auto& stack_entry = state.stack.back();
    ++stack_entry.position;
    if (state.get_end_array()) {
        if (stack_entry.position != 2)
            return eosio::from_json_error::expected_variant;
        if (trace_json_to_bin)
            printf("%*s]\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return eosio::outcome::success();
    }
    const std::vector<eosio::abi_field>& fields = *stack_entry.type->as_variant();
    if (stack_entry.position == 0) {
        OUTCOME_TRY(typeName, state.get_string());
        if (trace_json_to_bin)
            printf("%*stype: %.*s\n", int(state.stack.size() * 4), "", (int)typeName.size(), typeName.data());
        auto it = std::find_if(fields.begin(), fields.end(),
                               [&](auto& field) { return field.name == typeName; });
        if (it == fields.end())
            return eosio::from_json_error::invalid_type_for_variant;
        stack_entry.variant_type_index = it - fields.begin();
        OUTCOME_TRY(eosio::varuint32_to_bin(stack_entry.variant_type_index, state.writer));
        return eosio::outcome::success();
    } else if (stack_entry.position == 1) {
        auto& field = fields[stack_entry.variant_type_index];
        return field.type->ser->json_to_bin(state, allow_extensions, field.type, true);
    } else {
        return eosio::from_json_error::expected_variant;
    }
}

///////////////////////////////////////////////////////////////////////////////
// bin_to_json
///////////////////////////////////////////////////////////////////////////////

template<typename F>
inline eosio::result<void> bin_to_json(eosio::input_stream& bin, const abi_type* type, std::string& dest, F&& f) {
    // FIXME: Write directly to the string instead of creating an additional buffer
    std::vector<char> buffer;
    eosio::vector_stream writer{buffer};
    bin_to_json_state state{bin, writer};
    OUTCOME_TRY(type->ser->bin_to_json(state, true, type, true));
    while (!state.stack.empty()) {
        f();
        auto& entry = state.stack.back();
        OUTCOME_TRY(entry.type->ser->bin_to_json(state, entry.allow_extensions, entry.type, false));
        if (state.stack.size() > max_stack_size)
            return eosio::abi_error::recursion_limit_reached;
    }
    dest = std::string_view(writer.data.data(), writer.data.size());
    return eosio::outcome::success();
}

inline eosio::result<void> bin_to_json(bin_to_json_state& state, bool allow_extensions, const abi_type* type, bool start) {
    return type->ser->bin_to_json(state, allow_extensions, type, start);
}

inline eosio::result<void> bin_to_json(pseudo_optional*, bin_to_json_state& state, bool allow_extensions,
                                       const abi_type* type, bool) {
    bool present;
    OUTCOME_TRY(from_bin(present, state.bin));
    if (present)
        return bin_to_json(state, allow_extensions, type->optional_of(), true);
    return state.writer.write("null", 4);
}

inline eosio::result<void> bin_to_json(pseudo_extension*, bin_to_json_state& state, bool allow_extensions,
                                       const abi_type* type, bool) {
    return bin_to_json(state, allow_extensions, type->extension_of(), true);
}

inline eosio::result<void> bin_to_json(pseudo_object*, bin_to_json_state& state, bool allow_extensions,
                                       const abi_type* type, bool start) {
    if (start) {
        if (trace_bin_to_json)
            printf("%*s{ %d fields\n", int(state.stack.size() * 4), "", int(type->as_struct()->fields.size()));
        state.stack.push_back({type, allow_extensions});
        OUTCOME_TRY(state.writer.write('{'));
        return eosio::outcome::success();
    }
    auto& stack_entry = state.stack.back();
    const std::vector<eosio::abi_field>& fields = type->as_struct()->fields;
    if (++stack_entry.position < (ptrdiff_t)fields.size()) {
        auto& field = fields[stack_entry.position];
        if (trace_bin_to_json)
            printf("%*sfield %d/%d: %s\n", int(state.stack.size() * 4), "", int(stack_entry.position),
                   int(fields.size()), std::string{field.name}.c_str());
        if (state.bin.pos == state.bin.end && field.type->extension_of() && allow_extensions) {
            state.skipped_extension = true;
            return eosio::outcome::success();
        }
        if(stack_entry.position != 0) { OUTCOME_TRY(state.writer.write(',')); };
        OUTCOME_TRY(to_json(field.name, state.writer));
        OUTCOME_TRY(state.writer.write(':'));
        return bin_to_json(state, allow_extensions && &field == &fields.back(), field.type, true);
    } else {
        if (trace_bin_to_json)
            printf("%*s}\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        OUTCOME_TRY(state.writer.write('}'));
        return eosio::outcome::success();
    }
}

inline eosio::result<void> bin_to_json(pseudo_array*, bin_to_json_state& state, bool, const abi_type* type,
                                         bool start) {
    if (start) {
        state.stack.push_back({type, false});
        OUTCOME_TRY(varuint32_from_bin(state.stack.back().array_size, state.bin));
        if (trace_bin_to_json)
            printf("%*s[ %d items\n", int(state.stack.size() * 4), "", int(state.stack.back().array_size));
        return state.writer.write('[');
    }
    auto& stack_entry = state.stack.back();
    if (++stack_entry.position < (ptrdiff_t)stack_entry.array_size) {
        if (trace_bin_to_json)
            printf("%*sitem %d/%d %p %s\n", int(state.stack.size() * 4), "", int(stack_entry.position),
                   int(stack_entry.array_size), type->array_of()->ser, type->array_of()->name.c_str());
        if (stack_entry.position != 0) { OUTCOME_TRY(state.writer.write(',')); }
        return bin_to_json(state, false, type->array_of(), true);
    } else {
        if (trace_bin_to_json)
            printf("%*s]\n", int((state.stack.size()) * 4), "");
        state.stack.pop_back();
        return state.writer.write(']');
    }
}

inline eosio::result<void> bin_to_json(pseudo_variant*, bin_to_json_state& state, bool allow_extensions,
                                         const abi_type* type, bool start) {
    if (start) {
        state.stack.push_back({type, allow_extensions});
        if (trace_bin_to_json)
            printf("%*s[ variant\n", int(state.stack.size() * 4), "");
        return state.writer.write('[');
    }
    auto& stack_entry = state.stack.back();
    if (++stack_entry.position == 0) {
        uint32_t index;
        OUTCOME_TRY(varuint32_from_bin(index, state.bin));
        const std::vector<eosio::abi_field>& fields = *stack_entry.type->as_variant();
        if (index >= fields.size())
            return eosio::stream_error::bad_variant_index;
        auto& f = fields[index];
        OUTCOME_TRY(to_json(f.name, state.writer));
        OUTCOME_TRY(state.writer.write(','));
        // FIXME: allow_extensions should be stack_entry.allow_extensions, so why are we combining them?
        return bin_to_json(state, allow_extensions && stack_entry.allow_extensions, f.type, true);
    } else {
        if (trace_bin_to_json)
            printf("%*s]\n", int((state.stack.size()) * 4), "");
        state.stack.pop_back();
        return state.writer.write(']');
    }
}

template <typename T>
auto bin_to_json(T* t, bin_to_json_state& state, bool, const abi_type*, bool start)
    -> eosio::result<std::void_t<decltype(from_bin(*t, state.bin)), decltype(to_json(*t, state.writer))>> {
    T v;
    OUTCOME_TRY(from_bin(v, state.bin));
    return to_json(v, state.writer);
}

} // namespace abieos
