#pragma once

#include <cstdint>
#include <string>

namespace eosio {

inline constexpr const char* get_type_name(bool*) { return "bool"; }
inline constexpr const char* get_type_name(std::int8_t*) { return "int8"; }
inline constexpr const char* get_type_name(std::uint8_t*) { return "uint8"; }
inline constexpr const char* get_type_name(std::int16_t*) { return "int16"; }
inline constexpr const char* get_type_name(std::uint16_t*) { return "uint16"; }
inline constexpr const char* get_type_name(std::int32_t*) { return "int32"; }
inline constexpr const char* get_type_name(std::uint32_t*) { return "uint32"; }
inline constexpr const char* get_type_name(std::int64_t*) { return "int64"; }
inline constexpr const char* get_type_name(std::uint64_t*) { return "uint64"; }
inline constexpr const char* get_type_name(float*) { return "float32"; }
inline constexpr const char* get_type_name(double*) { return "float64"; }
inline constexpr const char* get_type_name(std::string*) { return "string"; }

} // namespace eosio
