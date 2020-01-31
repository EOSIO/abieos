// copyright defined in abieos/LICENSE.txt

#pragma once

#include <algorithm>
#include <array>
#include <stdint.h>
#include <string>
#include <string_view>
#include <eosio/eosio_outcome.hpp>
#include <eosio/from_json.hpp>

#include "abieos_ripemd160.hpp"

#define ABIEOS_NODISCARD [[nodiscard]]

namespace abieos {

template <typename State>
ABIEOS_NODISCARD bool set_error(State& state, std::string error) {
    state.error = std::move(error);
    return false;
}

ABIEOS_NODISCARD inline bool set_error(std::string& state, std::string error) {
    state = std::move(error);
    return false;
}

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
bool is_negative(const std::array<uint8_t, size>& a) {
    return a[size - 1] & 0x80;
}

template <auto size>
void negate(std::array<uint8_t, size>& a) {
    uint8_t carry = 1;
    for (auto& byte : a) {
        int x = uint8_t(~byte) + carry;
        byte = x;
        carry = x >> 8;
    }
}

template <auto size>
ABIEOS_NODISCARD inline eosio::result<void> decimal_to_binary(std::array<uint8_t, size>& result,
                                                              std::string_view s) {
    memset(result.begin(), 0, result.size());
    for (auto& src_digit : s) {
        if (src_digit < '0' || src_digit > '9')
            return eosio::from_json_error::expected_int;
        uint8_t carry = src_digit - '0';
        for (auto& result_byte : result) {
            int x = result_byte * 10 + carry;
            result_byte = x;
            carry = x >> 8;
        }
        if (carry)
            return eosio::from_json_error::number_out_of_range;
    }
    return eosio::outcome::success();
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

ABIEOS_NODISCARD inline bool base58_to_binary(std::vector<uint8_t>& result, std::string& error, std::string_view s) {
    result.clear();
    for (auto& src_digit : s) {
        int carry = base58_map[src_digit];
        if (carry < 0)
            return set_error(error, "invalid base-58 value");
        for (auto& result_byte : result) {
            int x = result_byte * 58 + carry;
            result_byte = x;
            carry = x >> 8;
        }
        if (carry)
            result.push_back(carry);
    }
    for (auto& src_digit : s)
        if (src_digit == '1')
            result.push_back(0);
        else
            break;
    std::reverse(result.begin(), result.end());
    return true;
}

template <typename Container>
std::string binary_to_base58(const Container& bin) {
    std::string result("");
    for (auto byte : bin) {
        static_assert(sizeof(byte) == 1);
        int carry = static_cast<uint8_t>(byte);
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

enum class key_type : uint8_t {
    k1 = 0,
    r1 = 1,
    wa = 2,
};

struct public_key {
    static constexpr int k1r1_size = 33;

    key_type type = key_type::k1;
    std::vector<uint8_t> data = std::vector<uint8_t>(k1r1_size);
};

struct private_key {
    static constexpr int k1r1_size = 32;

    key_type type = key_type::k1;
    std::vector<uint8_t> data = std::vector<uint8_t>(k1r1_size);
};

struct signature {
    static constexpr int k1r1_size = 65;

    key_type type = key_type::k1;
    std::vector<uint8_t> data = std::vector<uint8_t>(k1r1_size);
};

ABIEOS_NODISCARD inline bool digest_message_ripemd160(std::array<unsigned char, 20>& digest, std::string& error,
                                                      const unsigned char* message, size_t message_len) {
    abieos_ripemd160::ripemd160_state self;
    abieos_ripemd160::ripemd160_init(&self);
    abieos_ripemd160::ripemd160_update(&self, message, message_len);
    if (!abieos_ripemd160::ripemd160_digest(&self, digest.data()))
        return set_error(error, "ripemd failed");
    return true;
}

template <int suffix_size, typename Container>
ABIEOS_NODISCARD inline bool digest_suffix_ripemd160(std::array<unsigned char, 20>& digest, std::string& error,
                                                     const Container& data, const char (&suffix)[suffix_size]) {
    abieos_ripemd160::ripemd160_state self;
    abieos_ripemd160::ripemd160_init(&self);
    abieos_ripemd160::ripemd160_update(&self, data.data(), data.size());
    abieos_ripemd160::ripemd160_update(&self, (uint8_t*)suffix, suffix_size - 1);
    if (!abieos_ripemd160::ripemd160_digest(&self, digest.data()))
        return set_error(error, "ripemd failed");
    return true;
}

template <typename Key, int suffix_size>
ABIEOS_NODISCARD bool string_to_key(Key& result, std::string& error, std::string_view s, key_type type,
                                    const char (&suffix)[suffix_size]) {
    std::vector<uint8_t> whole;
    if (!base58_to_binary(whole, error, s))
        return false;
    if (whole.size() <= 4)
        return set_error(error, "key has invalid size");
    result.type = type;
    result.data.resize(whole.size() - 4);
    memcpy(result.data.data(), whole.data(), result.data.size());
    std::array<unsigned char, 20> ripe_digest;
    if (!digest_suffix_ripemd160(ripe_digest, error, result.data, suffix))
        return false;
    if (memcmp(ripe_digest.data(), whole.data() + result.data.size(), 4))
        return set_error(error, "checksum doesn't match");
    return true;
}

template <typename Key, int suffix_size>
ABIEOS_NODISCARD bool key_to_string(std::string& dest, std::string& error, const Key& key,
                                    const char (&suffix)[suffix_size], const char* prefix) {
    std::array<unsigned char, 20> ripe_digest;
    if (!digest_suffix_ripemd160(ripe_digest, error, key.data, suffix))
        return false;
    std::vector<uint8_t> whole(key.data.size() + 4);
    memcpy(whole.data(), key.data.data(), key.data.size());
    memcpy(whole.data() + key.data.size(), ripe_digest.data(), 4);
    dest = prefix + binary_to_base58(whole);
    return true;
}

ABIEOS_NODISCARD inline bool string_to_public_key(public_key& dest, std::string& error, std::string_view s) {
    if (s.size() >= 3 && s.substr(0, 3) == "EOS") {
        std::vector<uint8_t> whole;
        if (!base58_to_binary(whole, error, s.substr(3)))
            return false;
        public_key key{key_type::k1};
        if (whole.size() != 37)
            return set_error(error, "Key has invalid size");
        key.data.resize(whole.size() - 4);
        memcpy(key.data.data(), whole.data(), key.data.size());
        std::array<unsigned char, 20> ripe_digest;
        if (!digest_message_ripemd160(ripe_digest, error, key.data.data(), key.data.size()))
            return false;
        if (memcmp(ripe_digest.data(), whole.data() + key.data.size(), 4))
            return set_error(error, "Key checksum doesn't match");
        dest = key;
        return true;
    } else if (s.size() >= 7 && s.substr(0, 7) == "PUB_K1_") {
        return string_to_key(dest, error, s.substr(7), key_type::k1, "K1");
    } else if (s.size() >= 7 && s.substr(0, 7) == "PUB_R1_") {
        return string_to_key(dest, error, s.substr(7), key_type::r1, "R1");
    } else if (s.size() >= 7 && s.substr(0, 7) == "PUB_WA_") {
        return string_to_key(dest, error, s.substr(7), key_type::wa, "WA");
    } else {
        return set_error(error, "unrecognized public key format");
    }
}

ABIEOS_NODISCARD inline bool public_key_to_string(std::string& dest, std::string& error, const public_key& key) {
    if (key.type == key_type::k1) {
        return key_to_string(dest, error, key, "K1", "PUB_K1_");
    } else if (key.type == key_type::r1) {
        return key_to_string(dest, error, key, "R1", "PUB_R1_");
    } else if (key.type == key_type::wa) {
        return key_to_string(dest, error, key, "WA", "PUB_WA_");
    } else {
        return set_error(error, "unrecognized public key format");
    }
}

ABIEOS_NODISCARD inline bool string_to_private_key(private_key& dest, std::string& error, std::string_view s) {
    if (s.size() >= 7 && s.substr(0, 7) == "PVT_R1_")
        return string_to_key(dest, error, s.substr(7), key_type::r1, "R1");
    else if (s.size() >= 4 && s.substr(0, 4) == "PVT_")
        return set_error(error, "unrecognized private key format");
    else {
        std::vector<uint8_t> whole;
        if (!base58_to_binary(whole, error, s))
            return false;
        private_key key{key_type::k1};
        if (whole.size() != 37)
            return set_error(error, "key has invalid size");
        key.data.resize(whole.size() - 5);
        // todo: verify checksum
        memcpy(key.data.data(), whole.data() + 1, key.data.size());
        dest = key;
        return true;
    }
}

ABIEOS_NODISCARD inline bool private_key_to_string(std::string& dest, std::string& error,
                                                   const private_key& private_key) {
    if (private_key.type == key_type::k1)
        return key_to_string(dest, error, private_key, "K1", "PVT_K1_");
    else if (private_key.type == key_type::r1)
        return key_to_string(dest, error, private_key, "R1", "PVT_R1_");
    else
        return set_error(error, "unrecognized private key format");
}

ABIEOS_NODISCARD inline bool string_to_signature(signature& dest, std::string& error, std::string_view s) {
    if (s.size() >= 7 && s.substr(0, 7) == "SIG_K1_")
        return string_to_key(dest, error, s.substr(7), key_type::k1, "K1");
    else if (s.size() >= 7 && s.substr(0, 7) == "SIG_R1_")
        return string_to_key(dest, error, s.substr(7), key_type::r1, "R1");
    else if (s.size() >= 7 && s.substr(0, 7) == "SIG_WA_")
        return string_to_key(dest, error, s.substr(7), key_type::wa, "WA");
    else
        return set_error(error, "unrecognized signature format");
}

ABIEOS_NODISCARD inline bool signature_to_string(std::string& dest, std::string& error, const signature& signature) {
    if (signature.type == key_type::k1)
        return key_to_string(dest, error, signature, "K1", "SIG_K1_");
    else if (signature.type == key_type::r1)
        return key_to_string(dest, error, signature, "R1", "SIG_R1_");
    else if (signature.type == key_type::wa)
        return key_to_string(dest, error, signature, "WA", "SIG_WA_");
    else
        return set_error(error, "unrecognized signature format");
}

} // namespace abieos
