#include <eosio/to_key.hpp>
#include "abieos.hpp"

int error_count;

void report_error(const char* assertion, const char* file, int line) {
    if(error_count <= 20) {
       printf("%s:%d: failed %s\n", file, line, assertion);
    }
    ++error_count;
}

#define CHECK(...) do { if(__VA_ARGS__) {} else { report_error(#__VA_ARGS__, __FILE__, __LINE__); } } while(0)

using abieos::int128;
using abieos::uint128;
using abieos::varint32;
using abieos::varuint32;
using abieos::float128;
using abieos::time_point;
using abieos::time_point_sec;
using abieos::block_timestamp;
using abieos::bytes;
using abieos::checksum160;
using abieos::checksum256;
using abieos::checksum512;
using abieos::key_type;
using abieos::public_key;
using abieos::private_key;
using abieos::signature;
using abieos::symbol;
using abieos::symbol_code;
using abieos::asset;

using vec_type = std::vector<int>;
struct struct_type {
   std::vector<int> v;
   std::optional<int> o;
   std::variant<int, double> va;
};
EOSIO_REFLECT(struct_type, v, o, va);
EOSIO_COMPARE(struct_type);

template<typename T>
void test_key(const T& x, const T& y) {
   auto keyx = eosio::convert_to_key(x).value();
   auto keyy = eosio::convert_to_key(y).value();
   CHECK(std::lexicographical_compare(keyx.begin(), keyx.end(), keyy.begin(), keyy.end(), std::less<unsigned char>()) == (x < y));
   CHECK(std::lexicographical_compare(keyy.begin(), keyy.end(), keyx.begin(), keyx.end(), std::less<unsigned char>()) == (y < x));
}

void test_compare() {
   test_key(true, true);
   test_key(false, false);
   test_key(false, true);
   test_key(true, false);
   test_key(int8_t(0), int8_t(0));
   test_key(int8_t(-128), int8_t(0));
   test_key(int8_t(-128), int8_t(127));
   test_key(uint8_t(0), uint8_t(0));
   test_key(uint8_t(0), uint8_t(255));
   test_key(uint32_t(0), uint32_t(0));
   test_key(uint32_t(0), uint32_t(1));
   test_key(uint32_t(0xFF000000), uint32_t(0xFF));
   test_key(int32_t(0), int32_t(0));
   test_key(int32_t(0), int32_t(1));
   test_key(int32_t(0), int32_t(-1));
   test_key(int32_t(0x7F000000), int32_t(0x100000FF));
}

int main() {
   test_compare();
   if(error_count) return 1;
}
