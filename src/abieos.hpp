#include <boost/algorithm/hex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <ctime>
#include <map>
#include <vector>

#include "abieos_numeric.hpp"

#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace abieos {

inline constexpr bool trace_json_to_native = false;
inline constexpr bool trace_json_to_native_event = false;
inline constexpr bool trace_json_to_bin = false;
inline constexpr bool trace_json_to_bin_event = false;
inline constexpr bool trace_bin_to_json = false;

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

// Pseudo objects never exist, except in serialized form
struct pseudo_object;
struct pseudo_array;

template <typename T>
void push_raw(std::vector<char>& bin, const T& obj) {
    static_assert(std::is_trivially_copyable_v<T>);
    bin.insert(bin.end(), reinterpret_cast<const char*>(&obj), reinterpret_cast<const char*>(&obj + 1));
}

struct input_buffer {
    const char* pos = nullptr;
    const char* end = nullptr;
};

inline void read_bin(input_buffer& bin, void* dest, ptrdiff_t size) {
    if (bin.end - bin.pos < size)
        throw std::runtime_error("read past end");
    memcpy(dest, bin.pos, size);
    bin.pos += size;
}

template <typename T>
void read_bin(input_buffer& bin, T& dest) {
    static_assert(std::is_trivially_copyable_v<T>);
    read_bin(bin, &dest, sizeof(dest));
}

template <typename T>
T read_bin(input_buffer& bin) {
    T result{};
    read_bin(bin, result);
    return result;
}

uint32_t read_varuint(input_buffer& bin);

inline std::string read_string(input_buffer& bin) {
    // todo: check size against remaining bytes in bin before allocating memory
    auto size = read_varuint(bin);
    std::string result(size, 0);
    read_bin(bin, result.data(), size);
    return result;
}

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

bool receive_event(struct json_to_native_state&, event_type, bool start);
bool receive_event(struct json_to_bin_state&, event_type, bool start);
bool receive_event(struct bin_to_json_state&, event_type, bool start);

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

    bool Null() { return receive_event(get_derived(), event_type::received_null, get_start()); }
    bool Bool(bool v) {
        received_data.value_bool = v;
        return receive_event(get_derived(), event_type::received_bool, get_start());
    }
    bool Int(int v) { return Int64(v); }
    bool Uint(unsigned v) { return Uint64(v); }
    bool Int64(int64_t v) {
        received_data.value_int64 = v;
        return receive_event(get_derived(), event_type::received_int64, get_start());
    }
    bool Uint64(uint64_t v) {
        received_data.value_uint64 = v;
        return receive_event(get_derived(), event_type::received_uint64, get_start());
    }
    bool Double(double v) {
        received_data.value_double = v;
        return receive_event(get_derived(), event_type::received_double, get_start());
    }
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
// state and serializers
///////////////////////////////////////////////////////////////////////////////

struct size_insertion {
    size_t position = 0;
    uint32_t size = 0;
};

struct native_serializer;

struct native_stack_entry {
    void* obj = nullptr;
    const native_serializer* ser = nullptr;
    int position = 0;
};

struct json_to_bin_stack_entry {
    const struct abi_type* type = nullptr;
    int position = -1;
    size_t size_insertion_index = 0;
};

struct bin_to_json_stack_entry {
    const struct abi_type* type = nullptr;
    int position = -1;
    uint32_t array_size = 0;
};

struct json_to_native_state : json_reader_handler<json_to_native_state> {
    std::vector<native_stack_entry> stack;
};

struct bin_to_native_state {
    std::vector<native_stack_entry> stack;
};

struct json_to_bin_state : json_reader_handler<json_to_bin_state> {
    std::vector<char> bin;
    std::vector<size_insertion> size_insertions{};
    std::vector<json_to_bin_stack_entry> stack{};
};

struct bin_to_json_state : json_reader_handler<bin_to_json_state> {
    input_buffer& bin;
    rapidjson::Writer<rapidjson::StringBuffer>& writer;
    std::vector<bin_to_json_stack_entry> stack{};

    bin_to_json_state(input_buffer& bin, rapidjson::Writer<rapidjson::StringBuffer>& writer)
        : bin{bin}, writer{writer} {}
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
    virtual bool json_to_bin(json_to_bin_state&, const abi_type*, event_type, bool) const = 0;
    virtual bool bin_to_json(bin_to_json_state&, const abi_type*, bool) const = 0;
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
bool json_to_native(std::vector<T>& obj, json_to_native_state& state, event_type event, bool start);
template <typename First, typename Second>
bool json_to_native(std::pair<First, Second>& obj, json_to_native_state& state, event_type event, bool start);
bool json_to_native(std::string& obj, json_to_native_state& state, event_type event, bool start);
template <typename T>
bool json_to_native(root_object_wrapper<T>& obj, json_to_native_state& state, event_type event, bool start);

template <typename T>
auto json_to_bin(T*, json_to_bin_state& state, const abi_type*, event_type event, bool start)
    -> std::enable_if_t<std::is_arithmetic_v<T>, bool>;
bool json_to_bin(std::string*, json_to_bin_state& state, const abi_type*, event_type event, bool start);
bool json_to_bin(pseudo_object*, json_to_bin_state& state, const abi_type* type, event_type event, bool start);
bool json_to_bin(pseudo_array*, json_to_bin_state& state, const abi_type* type, event_type event, bool start);

template <typename T>
auto bin_to_json(T*, bin_to_json_state& state, const abi_type*, bool start)
    -> std::enable_if_t<std::is_arithmetic_v<T>, bool>;
bool bin_to_json(std::string*, bin_to_json_state& state, const abi_type*, bool start);
bool bin_to_json(pseudo_object*, bin_to_json_state& state, const abi_type* type, bool start);
bool bin_to_json(pseudo_array*, bin_to_json_state& state, const abi_type* type, bool start);

///////////////////////////////////////////////////////////////////////////////
// serializable types
///////////////////////////////////////////////////////////////////////////////

struct bytes {
    std::vector<char> data;
};

inline bool bin_to_native(bin_to_native_state& state, bytes& obj, bool start) { return true; }
inline bool json_to_native(bytes& obj, json_to_native_state& state, event_type event, bool start) {
    throw std::runtime_error("bytes not implemented");
}

void push_varuint32(std::vector<char>& bin, uint32_t v);

inline bool json_to_bin(bytes*, json_to_bin_state& state, const abi_type*, event_type event, bool start) {
    if (event == event_type::received_string) {
        auto& s = state.received_data.value_string;
        if (trace_json_to_bin)
            printf("%*sbytes (%d hex digits)\n", int(state.stack.size() * 4), "", int(s.size()));
        if (s.size() & 1)
            throw std::runtime_error("odd number of hex digits");
        push_varuint32(state.bin, s.size() / 2);
        boost::algorithm::unhex(s.begin(), s.end(), std::back_inserter(state.bin));
        return true;
    } else
        throw std::runtime_error("expected string containing hex digits");
}

inline bool bin_to_json(bytes*, bin_to_json_state& state, const abi_type*, bool start) {
    // todo: check size against remaining bytes in bin before allocating memory
    auto size = read_varuint(state.bin);
    std::vector<char> raw(size);
    read_bin(state.bin, raw.data(), size);
    std::string result;
    boost::algorithm::hex(raw.begin(), raw.end(), std::back_inserter(result));
    return state.writer.String(result.c_str(), result.size());
}

template <unsigned size>
struct checksum {
    std::array<uint8_t, size> value{{0}};
};

using checksum160 = checksum<20>;
using checksum256 = checksum<32>;
using checksum512 = checksum<64>;

template <unsigned size>
inline bool json_to_bin(checksum<size>*, json_to_bin_state& state, const abi_type*, event_type event, bool start) {
    if (event == event_type::received_string) {
        auto& s = state.received_data.value_string;
        if (trace_json_to_bin)
            printf("%*schecksum\n", int(state.stack.size() * 4), "");
        std::vector<uint8_t> v;
        boost::algorithm::unhex(s.begin(), s.end(), std::back_inserter(v));
        if (v.size() != size)
            throw std::runtime_error("checksum has incorrect size");
        state.bin.insert(state.bin.end(), v.begin(), v.end());
        return true;
    } else
        throw std::runtime_error("expected string containing hex");
}

template <unsigned size>
inline bool bin_to_json(checksum<size>*, bin_to_json_state& state, const abi_type*, bool start) {
    auto v = read_bin<checksum<size>>(state.bin);
    std::string result;
    boost::algorithm::hex(v.value.begin(), v.value.end(), std::back_inserter(result));
    return state.writer.String(result.c_str(), result.size());
}

struct uint128 {
    std::array<uint8_t, 16> value{{0}};
};

inline bool json_to_bin(uint128*, json_to_bin_state& state, const abi_type*, event_type event, bool start) {
    if (event == event_type::received_string) {
        auto& s = state.received_data.value_string;
        if (trace_json_to_bin)
            printf("%*suint128\n", int(state.stack.size() * 4), "");
        auto value = decimal_to_binary<16>(s);
        push_raw(state.bin, value);
        return true;
    } else
        throw std::runtime_error("expected string containing uint128");
}

inline bool bin_to_json(uint128*, bin_to_json_state& state, const abi_type*, bool start) {
    auto v = read_bin<uint128>(state.bin);
    auto result = binary_to_decimal(v.value);
    return state.writer.String(result.c_str(), result.size());
}

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
        throw std::runtime_error("expected string containing name");
}

inline bool json_to_bin(name*, json_to_bin_state& state, const abi_type*, event_type event, bool start) {
    if (event == event_type::received_string) {
        name obj{string_to_name(state.received_data.value_string.c_str())};
        if (trace_json_to_bin)
            printf("%*sname: %s (%08llx) %s\n", int(state.stack.size() * 4), "",
                   state.received_data.value_string.c_str(), obj.value, std::string{obj}.c_str());
        push_raw(state.bin, obj.value);
        return true;
    } else
        throw std::runtime_error("expected string containing name");
}

inline bool bin_to_json(name*, bin_to_json_state& state, const abi_type*, bool start) {
    auto s = std::string{name{read_bin<uint64_t>(state.bin)}};
    return state.writer.String(s.c_str(), s.size());
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

inline void push_varuint32(std::vector<char>& bin, uint32_t v) {
    uint64_t val = v;
    do {
        uint8_t b = val & 0x7f;
        val >>= 7;
        b |= ((val > 0) << 7);
        bin.push_back(b);
    } while (val);
}

inline uint32_t read_varuint(input_buffer& bin) {
    uint32_t result = 0;
    int shift = 0;
    uint8_t b = 0;
    do {
        b = read_bin<uint8_t>(bin);
        result |= (b & 0x7f) << shift;
        shift += 7;
    } while (b & 0x80);
    return result;
}

inline bool json_to_bin(varuint32*, json_to_bin_state& state, const abi_type*, event_type event, bool start) {
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
        throw std::runtime_error("expected number");
    push_varuint32(state.bin, obj.value);
    return true;
}

inline bool bin_to_json(varuint32*, bin_to_json_state& state, const abi_type*, bool start) {
    return state.writer.Uint64(read_varuint(state.bin));
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
        return boost::posix_time::to_iso_extended_string(ptime) + ".000";
    }
};

inline bool json_to_bin(time_point_sec*, json_to_bin_state& state, const abi_type*, event_type event, bool start) {
    if (event == event_type::received_string) {
        time_point_sec obj{state.received_data.value_string};
        if (trace_json_to_bin)
            printf("%*stime_point_sec: %s (%u) %s\n", int(state.stack.size() * 4), "",
                   state.received_data.value_string.c_str(), (unsigned)obj.utc_seconds, std::string{obj}.c_str());
        push_raw(state.bin, obj.utc_seconds);
        return true;
    } else
        throw std::runtime_error("expected string containing time_point_sec");
}

inline bool bin_to_json(time_point_sec*, bin_to_json_state& state, const abi_type*, bool start) {
    auto s = std::string{time_point_sec{read_bin<uint32_t>(state.bin)}};
    return state.writer.String(s.c_str(), s.size());
}

struct symbol_code {
    uint64_t value = 0;
};

inline constexpr uint64_t string_to_symbol_code(const char* str) {
    while (*str == ' ')
        ++str;
    uint64_t result = 0;
    uint32_t i = 0;
    while (*str >= 'A' && *str <= 'Z')
        result |= uint64_t(*str++) << (8 * i++);
    return result;
}

inline std::string symbol_code_to_string(uint64_t v) {
    std::string result;
    while (v > 0) {
        result += char(v & 0xFF);
        v >>= 8;
    }
    return result;
}

inline bool json_to_bin(symbol_code*, json_to_bin_state& state, const abi_type*, event_type event, bool start) {
    if (event == event_type::received_string) {
        auto& s = state.received_data.value_string;
        if (trace_json_to_bin)
            printf("%*ssymbol_code: %s\n", int(state.stack.size() * 4), "", s.c_str());
        auto v = string_to_symbol_code(s.c_str());
        push_raw(state.bin, v);
        return true;
    } else
        throw std::runtime_error("expected string containing symbol_code");
}

inline bool bin_to_json(symbol_code*, bin_to_json_state& state, const abi_type*, bool start) {
    std::string result{symbol_code_to_string(read_bin<uint64_t>(state.bin))};
    return state.writer.String(result.c_str(), result.size());
}

struct symbol {
    uint64_t value = 0;
};

inline constexpr uint64_t string_to_symbol(uint8_t precision, const char* str) {
    return (string_to_symbol_code(str) << 8) | precision;
}

inline constexpr uint64_t string_to_symbol(const char* str) {
    uint8_t precision = 0;
    while (*str >= '0' && *str <= '9')
        precision = precision * 10 + (*str++ - '0');
    if (*str == ',')
        ++str;
    return string_to_symbol(precision, str);
}

inline std::string symbol_to_string(uint64_t v) {
    return std::to_string(v & 0xff) + "," + symbol_code_to_string(v >> 8);
}

inline bool json_to_bin(symbol*, json_to_bin_state& state, const abi_type*, event_type event, bool start) {
    if (event == event_type::received_string) {
        auto& s = state.received_data.value_string;
        if (trace_json_to_bin)
            printf("%*ssymbol: %s\n", int(state.stack.size() * 4), "", s.c_str());
        auto v = string_to_symbol(s.c_str());
        push_raw(state.bin, v);
        return true;
    } else
        throw std::runtime_error("expected string containing symbol");
}

inline bool bin_to_json(symbol*, bin_to_json_state& state, const abi_type*, bool start) {
    std::string result{symbol_to_string(read_bin<uint64_t>(state.bin))};
    return state.writer.String(result.c_str(), result.size());
}

struct asset {
    int64_t amount = 0;
    symbol sym{};
};

inline asset string_to_asset(const char* s) {
    // todo: check overflow
    while (*s == ' ')
        ++s;
    uint64_t amount = 0;
    uint8_t precision = 0;
    bool negative = false;
    if (*s == '-') {
        ++s;
        negative = true;
    }
    while (*s >= '0' && *s <= '9')
        amount = amount * 10 + (*s++ - '0');
    if (*s == '.') {
        ++s;
        while (*s >= '0' && *s <= '9') {
            amount = amount * 10 + (*s++ - '0');
            ++precision;
        }
    }
    if (negative)
        amount = -amount;
    auto code = string_to_symbol_code(s);
    return asset{(int64_t)amount, symbol{(code << 8) | precision}};
}

inline std::string asset_to_string(const asset& v) {
    std::string result;
    uint64_t amount;
    if (v.amount < 0)
        amount = -v.amount;
    else
        amount = v.amount;
    uint8_t precision = v.sym.value;
    if (precision) {
        while (precision--) {
            result += '0' + amount % 10;
            amount /= 10;
        }
        result += '.';
    }
    do {
        result += '0' + amount % 10;
        amount /= 10;
    } while (amount);
    if (v.amount < 0)
        result += '-';
    std::reverse(result.begin(), result.end());
    return result + ' ' + symbol_code_to_string(v.sym.value >> 8);
}

inline bool json_to_bin(asset*, json_to_bin_state& state, const abi_type*, event_type event, bool start) {
    if (event == event_type::received_string) {
        auto& s = state.received_data.value_string;
        if (trace_json_to_bin)
            printf("%*sasset: %s\n", int(state.stack.size() * 4), "", s.c_str());
        auto v = string_to_asset(s.c_str());
        push_raw(state.bin, v.amount);
        push_raw(state.bin, v.sym.value);
        return true;
    } else
        throw std::runtime_error("expected string containing asset");
}

inline bool bin_to_json(asset*, bin_to_json_state& state, const abi_type*, bool start) {
    asset v{};
    read_bin(state.bin, v.amount);
    read_bin(state.bin, v.sym.value);
    auto s = asset_to_string(v);
    return state.writer.String(s.c_str(), s.size());
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
    f("type", member_ptr<&table_def::type>{}, true);
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
    if (stack_entry.position < (ptrdiff_t)native_field_serializers_for<T>.size()) {
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
    if (stack_entry.position < (ptrdiff_t)v.size()) {
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

inline bool receive_event(struct json_to_native_state& state, event_type event, bool) {
    if (state.stack.empty())
        throw std::runtime_error("extra data");
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
        throw std::runtime_error("expected number or boolean");
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
        throw std::runtime_error("expected object");
}

template <typename T>
auto json_to_native(T& obj, json_to_native_state& state, event_type event, bool start)
    -> std::enable_if_t<std::is_class_v<T>, bool> {

    if (start) {
        if (event != event_type::received_start_object)
            throw std::runtime_error("expected object");
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
        while (stack_entry.position < (ptrdiff_t)native_field_serializers_for<T>.size() &&
               native_field_serializers_for<T>[stack_entry.position].name != state.received_data.key)
            ++stack_entry.position;
        if (stack_entry.position >= (ptrdiff_t)native_field_serializers_for<T>.size())
            throw std::runtime_error("unknown field " + state.received_data.key); // TODO: eat unknown subtree
        return true;
    } else if (stack_entry.position < (ptrdiff_t)native_field_serializers_for<T>.size()) {
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
bool json_to_native(std::vector<T>& v, json_to_native_state& state, event_type event, bool start) {
    if (start) {
        if (event != event_type::received_start_array)
            throw std::runtime_error("expected array");
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
bool json_to_native(std::pair<First, Second>& obj, json_to_native_state& state, event_type event, bool start) {
    throw std::runtime_error("pair not implemented"); // TODO
}

inline bool json_to_native(std::string& obj, json_to_native_state& state, event_type event, bool start) {
    if (event == event_type::received_string) {
        obj = state.received_data.value_string;
        if (trace_json_to_native)
            printf("%*sstring: %s\n", int(state.stack.size() * 4), "", obj.c_str());
        return true;
    } else
        throw std::runtime_error("expected string");
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
    f("uint128", (uint128*)nullptr);
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
    f("checksum160", (checksum160*)nullptr);
    f("checksum256", (checksum256*)nullptr);
    f("checksum512", (checksum512*)nullptr);
    // f("public_key", (public_key_type*)nullptr); !!!
    // f("signature", (signature_type*)nullptr); !!!
    f("symbol", (symbol*)nullptr);
    f("symbol_code", (symbol_code*)nullptr);
    f("asset", (asset*)nullptr);
    // f("extended_asset", (extended_asset*)nullptr);
}

template <typename T>
struct abi_serializer_impl : abi_serializer {
    bool json_to_bin(json_to_bin_state& state, const abi_type* type, event_type event, bool start) const override {
        return ::abieos::json_to_bin((T*)nullptr, state, type, event, start);
    }
    bool bin_to_json(bin_to_json_state& state, const abi_type* type, bool start) const override {
        return ::abieos::bin_to_json((T*)nullptr, state, type, start);
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
            type.ser = &abi_serializer_for<pseudo_array>;
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
        abi_type type{s.name};
        type.struct_def = &s;
        type.ser = &abi_serializer_for<pseudo_object>;
        auto [_, inserted] = c.abi_types.insert({s.name, std::move(type)});
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

inline bool receive_event(struct json_to_bin_state& state, event_type event, bool start) {
    if (state.stack.empty())
        return false;
    if (trace_json_to_bin_event)
        printf("(event %d %d)\n", event, start);
    auto* type = state.stack.back().type;
    if (start)
        state.stack.clear();
    if (type->ser)
        return type->ser->json_to_bin(state, type, event, start);
    else
        return false;
}

inline bool json_to_bin(std::vector<char>& bin, const abi_type* type, std::string_view json) {
    std::string mutable_json{json};
    json_to_bin_state state;
    state.stack.push_back({type});
    rapidjson::Reader reader;
    rapidjson::InsituStringStream ss(mutable_json.data());
    if (!reader.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag |
                      rapidjson::kParseFullPrecisionFlag>(ss, state))
        return false;
    size_t pos = 0;
    for (auto& insertion : state.size_insertions) {
        bin.insert(bin.end(), state.bin.begin() + pos, state.bin.begin() + insertion.position);
        push_varuint32(bin, insertion.size);
        pos = insertion.position;
    }
    bin.insert(bin.end(), state.bin.begin() + pos, state.bin.end());
    return true;
}

inline bool json_to_bin(pseudo_object*, json_to_bin_state& state, const abi_type* type, event_type event, bool start) {
    if (start) {
        if (event != event_type::received_start_object)
            return false;
        if (trace_json_to_bin)
            printf("%*s{ %d fields\n", int(state.stack.size() * 4), "", int(type->fields.size()));
        state.stack.push_back({type});
        return true;
    }
    auto& stack_entry = state.stack.back();
    if (event == event_type::received_end_object) {
        if (stack_entry.position + 1 != (ptrdiff_t)type->fields.size())
            throw std::runtime_error("expected field \"" + type->fields[stack_entry.position + 1].name + "\"");
        if (trace_json_to_bin)
            printf("%*s}\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        return true;
    }
    if (event == event_type::received_key) {
        if (++stack_entry.position >= (ptrdiff_t)type->fields.size())
            return false; // todo: missing fields
        auto& field = type->fields[stack_entry.position];
        if (state.received_data.key != field.name)
            throw std::runtime_error("expected field \"" + field.name + "\"");
        return true;
    } else {
        auto& field = type->fields[stack_entry.position];
        if (trace_json_to_bin)
            printf("%*sfield %d/%d: %s (event %d)\n", int(state.stack.size() * 4), "", int(stack_entry.position),
                   int(type->fields.size()), std::string{field.name}.c_str(), event);
        if (field.type->ser)
            return field.type->ser->json_to_bin(state, field.type, event, true);
        else
            return false;
    }
}

inline bool json_to_bin(pseudo_array*, json_to_bin_state& state, const abi_type* type, event_type event, bool start) {
    if (start) {
        if (event != event_type::received_start_array)
            return false;
        if (trace_json_to_bin)
            printf("%*s[\n", int(state.stack.size() * 4), "");
        state.stack.push_back({type});
        state.stack.back().size_insertion_index = state.size_insertions.size();
        state.size_insertions.push_back({state.bin.size()});
        return true;
    }
    auto& stack_entry = state.stack.back();
    if (event == event_type::received_end_array) {
        if (trace_json_to_bin)
            printf("%*s]\n", int((state.stack.size() - 1) * 4), "");
        state.size_insertions[stack_entry.size_insertion_index].size = stack_entry.position + 1;
        state.stack.pop_back();
        return true;
    }
    ++stack_entry.position;
    if (trace_json_to_bin)
        printf("%*sitem (event %d)\n", int(state.stack.size() * 4), "", event);
    if (type->array_of->ser)
        return type->array_of->ser->json_to_bin(state, type->array_of, event, true);
    else
        return false;
}

template <typename T>
auto json_to_bin(T*, json_to_bin_state& state, const abi_type*, event_type event, bool start)
    -> std::enable_if_t<std::is_arithmetic_v<T>, bool> {
    T obj;
    if (event == event_type::received_bool) {
        obj = state.received_data.value_bool;
    } else if (event == event_type::received_uint64) {
        obj = state.received_data.value_uint64;
    } else if (event == event_type::received_int64) {
        obj = state.received_data.value_int64;
    } else if (event == event_type::received_double) {
        obj = state.received_data.value_double;
    } else if (event == event_type::received_string && std::is_integral_v<T> && std::is_signed_v<T>) {
        obj = stoll(state.received_data.value_string);
    } else if (event == event_type::received_string && std::is_integral_v<T> && !std::is_signed_v<T>) {
        obj = stoull(state.received_data.value_string);
    } else {
        return false;
    }
    push_raw(state.bin, obj);
    return true;
}

inline bool json_to_bin(std::string*, json_to_bin_state& state, const abi_type*, event_type event, bool start) {
    if (event == event_type::received_string) {
        auto& s = state.received_data.value_string;
        if (trace_json_to_bin)
            printf("%*sstring: %s\n", int(state.stack.size() * 4), "", s.c_str());
        push_varuint32(state.bin, s.size());
        state.bin.insert(state.bin.end(), s.begin(), s.end());
        return true;
    } else
        return false;
}

///////////////////////////////////////////////////////////////////////////////
// bin_to_json
///////////////////////////////////////////////////////////////////////////////

inline bool bin_to_json(input_buffer& bin, const abi_type* type, std::string& dest) {
    if (!type->ser)
        return false;
    rapidjson::StringBuffer buffer{};
    rapidjson::Writer<rapidjson::StringBuffer> writer{buffer};
    bin_to_json_state state{bin, writer};
    if (!type->ser->bin_to_json(state, type, true))
        return false;
    if (type->struct_def || type->array_of)
        while (!state.stack.empty())
            if (!state.stack.back().type->ser->bin_to_json(state, state.stack.back().type, false))
                return false;
    dest = buffer.GetString();
    return true;
}

inline bool bin_to_json(pseudo_object*, bin_to_json_state& state, const abi_type* type, bool start) {
    if (start) {
        if (trace_bin_to_json)
            printf("%*s{ %d fields\n", int(state.stack.size() * 4), "", int(type->fields.size()));
        state.stack.push_back({type});
        state.writer.StartObject();
        return true;
    }
    auto& stack_entry = state.stack.back();
    if (++stack_entry.position < (ptrdiff_t)type->fields.size()) {
        auto& field = type->fields[stack_entry.position];
        if (trace_bin_to_json)
            printf("%*sfield %d/%d: %s\n", int(state.stack.size() * 4), "", int(stack_entry.position),
                   int(type->fields.size()), std::string{field.name}.c_str());
        state.writer.Key(field.name.c_str(), field.name.length());
        if (field.type->ser)
            return field.type->ser->bin_to_json(state, field.type, true);
        else
            return false;
        return true;
    } else {
        if (trace_bin_to_json)
            printf("%*s}\n", int((state.stack.size() - 1) * 4), "");
        state.stack.pop_back();
        state.writer.EndObject();
        return true;
    }
}

inline bool bin_to_json(pseudo_array*, bin_to_json_state& state, const abi_type* type, bool start) {
    if (start) {
        state.stack.push_back({type});
        state.stack.back().array_size = read_varuint(state.bin);
        if (trace_bin_to_json)
            printf("%*s[ %d items\n", int(state.stack.size() * 4), "", int(state.stack.back().array_size));
        state.writer.StartArray();
        return true;
    }
    auto& stack_entry = state.stack.back();
    if (++stack_entry.position < (ptrdiff_t)stack_entry.array_size) {
        if (trace_bin_to_json)
            printf("%*sitem %d/%d %p %s\n", int(state.stack.size() * 4), "", int(stack_entry.position),
                   int(stack_entry.array_size), type->array_of->ser, type->array_of->name.c_str());
        if (type->array_of->ser)
            return type->array_of->ser->bin_to_json(state, type->array_of, true);
        else
            return false;
    } else {
        if (trace_bin_to_json)
            printf("%*s]\n", int((state.stack.size()) * 4), "");
        state.stack.pop_back();
        state.writer.EndArray();
        return true;
    }
}

template <typename T>
auto bin_to_json(T*, bin_to_json_state& state, const abi_type*, bool start)
    -> std::enable_if_t<std::is_arithmetic_v<T>, bool> {

    if constexpr (std::is_same_v<T, bool>) {
        return state.writer.Bool(read_bin<T>(state.bin));
    } else if constexpr (std::is_floating_point_v<T>) {
        return state.writer.Double(read_bin<T>(state.bin));
    } else if constexpr (sizeof(T) == 8) {
        auto s = std::to_string(read_bin<T>(state.bin));
        return state.writer.String(s.c_str(), s.size());
    } else if constexpr (std::is_signed_v<T>) {
        return state.writer.Int64(read_bin<T>(state.bin));
    } else {
        return state.writer.Uint64(read_bin<T>(state.bin));
    }
}

inline bool bin_to_json(std::string*, bin_to_json_state& state, const abi_type*, bool start) {
    // 64 bit: use string !!!
    auto s = read_string(state.bin);
    return state.writer.String(s.c_str(), s.size());
}

} // namespace abieos
