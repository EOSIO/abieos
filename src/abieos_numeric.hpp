#include <algorithm>
#include <array>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>

#include "ripemd160.hpp"

namespace abieos {

inline constexpr char base58_chars[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

inline constexpr auto create_base58_map() {
    std::array<int8_t, 256> base58_map{{0}};
    for (unsigned i = 0; i < base58_map.size(); ++i)
        base58_map[i] = -1;
    for (unsigned i = 0; i < sizeof(base58_chars); ++i)
        base58_map[base58_chars[i]] = i;
    return base58_map;
}

inline constexpr auto base58_map = create_base58_map();

template <auto size>
std::array<uint8_t, size> decimal_to_binary(std::string_view s) {
    std::array<uint8_t, size> result{{0}};
    for (auto& src_digit : s) {
        if (src_digit < '0' || src_digit > '9')
            throw std::runtime_error("invalid number");
        uint8_t carry = src_digit - '0';
        for (auto& result_byte : result) {
            int x = result_byte * 10 + carry;
            result_byte = x;
            carry = x >> 8;
        }
        if (carry)
            throw std::runtime_error("number is out of range");
    }
    return result;
}

template <auto size>
std::string binary_to_decimal(const std::array<uint8_t, size>& bin) {
    std::string result("0");
    for (auto byte_it = bin.rbegin(); byte_it != bin.rend(); ++byte_it) {
        int carry = *byte_it;
        for (auto& result_digit : result) {
            int x = ((result_digit - '0') << 8) + carry;
            result_digit = '0' + x % 10;
            carry = x / 10;
        }
        while (carry) {
            result.push_back('0' + carry % 10);
            carry = carry / 10;
        }
    }
    std::reverse(result.begin(), result.end());
    return result;
}

template <auto size>
std::array<uint8_t, size> base58_to_binary(std::string_view s) {
    std::array<uint8_t, size> result{{0}};
    for (auto& src_digit : s) {
        int carry = base58_map[src_digit];
        if (carry < 0)
            throw std::runtime_error("invalid base-58 value");
        for (auto& result_byte : result) {
            int x = result_byte * 58 + carry;
            result_byte = x;
            carry = x >> 8;
        }
        if (carry)
            throw std::runtime_error("base-58 value is out of range");
    }
    std::reverse(result.begin(), result.end());
    return result;
}

template <auto size>
std::string binary_to_base58(const std::array<uint8_t, size>& bin) {
    std::string result("");
    for (auto byte : bin) {
        int carry = byte;
        for (auto& result_digit : result) {
            int x = (base58_map[result_digit] << 8) + carry;
            result_digit = base58_chars[x % 58];
            carry = x / 58;
        }
        while (carry) {
            result.push_back(base58_chars[carry % 58]);
            carry = carry / 58;
        }
    }
    for (auto byte : bin)
        if (byte)
            break;
        else
            result.push_back('1');
    std::reverse(result.begin(), result.end());
    return result;
}

enum class public_key_type : uint8_t {
    k1 = 0,
    r1 = 1,
};

struct public_key {
    public_key_type type{};
    std::array<uint8_t, 33> data{};
};

inline auto digest_message_ripemd160(const unsigned char* message, size_t message_len) {
    std::array<unsigned char, 20> digest;
    ripemd160::ripemd160_state self;
    ripemd160::ripemd160_init(&self);
    ripemd160::ripemd160_update(&self, message, message_len);
    if (!ripemd160::ripemd160_digest(&self, digest.data()))
        throw std::runtime_error("ripemd failed");
    return digest;
}

inline auto digest_r1_ripemd160(const std::array<uint8_t, 33>& data) {
    std::array<uint8_t, 35> digest_data;
    memcpy(digest_data.data(), data.data(), data.size());
    digest_data[33] = 'R';
    digest_data[34] = '1';
    return digest_message_ripemd160(digest_data.data(), digest_data.size());
}

inline public_key string_to_public_key(const std::string& s) {
    if (s.size() >= 3 && std::string_view{s.c_str(), 3} == "EOS") {
        auto whole = base58_to_binary<37>({s.c_str() + 3, s.size() - 3});
        public_key key{public_key_type::k1};
        static_assert(whole.size() == key.data.size() + 4);
        memcpy(key.data.data(), whole.data(), key.data.size());
        auto ripe_digest = digest_message_ripemd160(key.data.data(), key.data.size());
        if (memcmp(ripe_digest.data(), whole.data() + key.data.size(), 4))
            throw std::runtime_error("Key checksum doesn't match");
        return key;
    } else if (s.size() >= 7 && std::string_view{s.c_str(), 7} == "PUB_R1_") {
        auto whole = base58_to_binary<37>({s.c_str() + 7, s.size() - 7});
        public_key key{public_key_type::r1};
        static_assert(whole.size() == key.data.size() + 4);
        memcpy(key.data.data(), whole.data(), key.data.size());
        auto ripe_digest = digest_r1_ripemd160(key.data);
        if (memcmp(ripe_digest.data(), whole.data() + key.data.size(), 4))
            throw std::runtime_error("Key checksum doesn't match");
        return key;
    } else {
        throw std::runtime_error("unrecognized public key format");
    }
}

inline std::string public_key_to_string(const public_key& key) {
    if (key.type == public_key_type::k1) {
        auto ripe_digest = digest_message_ripemd160(key.data.data(), key.data.size());
        std::array<uint8_t, 37> whole;
        memcpy(whole.data(), key.data.data(), key.data.size());
        memcpy(whole.data() + key.data.size(), ripe_digest.data(), 4);
        return "EOS" + binary_to_base58(whole);
    } else if (key.type == public_key_type::r1) {
        auto ripe_digest = digest_r1_ripemd160(key.data);
        std::array<uint8_t, 37> whole;
        memcpy(whole.data(), key.data.data(), key.data.size());
        memcpy(whole.data() + key.data.size(), ripe_digest.data(), 4);
        return "PUB_R1_" + binary_to_base58(whole);
    } else {
        throw std::runtime_error("unrecognized public key format");
    }
}

} // namespace abieos
