#include <algorithm>
#include <array>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>

namespace abieos {

template <auto size>
std::array<uint8_t, size> decimal_to_binary(std::string_view s) {
    std::array<uint8_t, size> result{{0}};
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (*it < '0' || *it > '9')
            throw std::runtime_error("invalid number");
        uint8_t carry = *it - '0';
        for (unsigned i = 0; i < size; ++i) {
            int x = result[i] * 10 + carry;
            result[i] = x;
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
    for (int i = (int)size * 8 - 1; i >= 0; --i) {
        int carry = (bin[i / 8] >> (i & 7)) & 1;
        for (size_t i = 0; i < result.size(); ++i) {
            int x = (result[i] - '0') * 2 + carry;
            result[i] = '0' + (x % 10);
            carry = x / 10;
        }
        if (carry)
            result.push_back('0' + carry);
    }
    std::reverse(result.begin(), result.end());
    return result;
}

} // namespace abieos
