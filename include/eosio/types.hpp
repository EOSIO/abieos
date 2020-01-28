#include <eosio/from_json.hpp>
#include <eosio/reflection.hpp>
#include <eosio/chain_conversions.hpp>

namespace eosio {

namespace abieos {

struct name {
    uint64_t value = 0;

    constexpr name() = default;
    constexpr explicit name(uint64_t value) : value{value} {}
    constexpr explicit name(const char* str) : value{eosio::string_to_name(str)} {}
    constexpr explicit name(std::string_view str) : value{eosio::string_to_name(str)} {}
    constexpr explicit name(const std::string& str) : value{eosio::string_to_name(str)} {}
    constexpr name(const name&) = default;

    explicit operator std::string() const { return eosio::name_to_string(value); }
};

inline bool operator==(name a, name b) { return a.value == b.value; }
inline bool operator!=(name a, name b) { return a.value != b.value; }
inline bool operator<(name a, name b) { return a.value < b.value; }

EOSIO_REFLECT(name, value);

template<typename S>
result<void> from_json(name& obj, S& stream) {
    OUTCOME_TRY(r, stream.get_string());
    if(!eosio::string_to_name_strict(r, obj.value))
        return from_json_error::invalid_name;
    return eosio::outcome::success();
}

template<typename S>
result<void> to_json(const name& obj, S& stream) {
    return to_json(eosio::name_to_string(obj.value), stream);
}

}

}
