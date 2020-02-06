#include <eosio/to_json.hpp>
#include <eosio/from_json.hpp>
#include <eosio/to_bin.hpp>
#include <eosio/from_bin.hpp>
#include "abieos.hpp"

int error_count;

void report_error(const char* assertion, const char* file, int line) {
    if(error_count <= 20) {
       printf("%s:%d: failed %s\n", file, line, assertion);
    }
    ++error_count;
}

#define CHECK(...) do { if(__VA_ARGS__) {} else { report_error(#__VA_ARGS__, __FILE__, __LINE__); } } while(0)

// Verify that serialization is consistent for vector_stream/size_stream/fixed_buf_stream
// and returns the serialized data.
template<typename T, typename F>
std::vector<char> test_serialize(const T& value, F&& f) {
   std::vector<char> buf1;
   eosio::vector_stream vecstream(buf1);
   eosio::size_stream szstream;
   CHECK(f(value, vecstream));
   CHECK(f(value, szstream));
   CHECK(szstream.size == vecstream.data.size());
   std::vector<char> buf2(szstream.size);
   eosio::fixed_buf_stream fxstream(buf2.data(), buf2.size());
   CHECK(f(value, fxstream));
   CHECK(buf1 == buf2);
   return buf1;
}

eosio::abi round_trip_abi(const eosio::abi& src) {
   eosio::abi_def def;
   CHECK(convert(src, def));
   eosio::abi result;
   CHECK(convert(def, result));
   return result;
}

template<typename T>
auto check_result(T&& t) {
   CHECK(t);
   if(t) return t.value();
   else return std::decay_t<decltype(t.value())>{};
}

// Verifies that all 6 conversions between native/bin/json round-trip
template<typename T>
void test(const T& value, eosio::abi& abi1, eosio::abi& abi2) {
   std::vector<char> bin = test_serialize(value, [](const T& v, auto& stream) { return to_bin(v, stream); });
   std::vector<char> json = test_serialize(value, [](const T& v, auto& stream) { return to_json(v, stream); });
   {
      T bin_value;
      eosio::input_stream bin_stream(bin);
      CHECK(from_bin(bin_value, bin_stream));
      CHECK(bin_value == value);
      T json_value;
      std::string mutable_json(json.data(), json.size());
      eosio::json_token_stream json_stream(mutable_json.data());
      CHECK(from_json(json_value, json_stream));
      CHECK(json_value == value);
   }

   for(eosio::abi* abi : {&abi1, &abi2})
   {
      // Get the ABI
      using eosio::get_type_name;
      const eosio::abi_type* type = abi->get_type(get_type_name((T*)nullptr)).value();

      // bin_to_json
      auto bin2 = check_result(type->json_to_bin({json.data(), json.size()}));
      CHECK(bin2 == bin);
      // json_to_bin
      eosio::input_stream bin_stream{bin};
      auto json2 = check_result(type->bin_to_json(bin_stream));
      CHECK(json2 == std::string(json.data(), json.size()));
   }
}

char empty_abi[] = R"({
    "version": "eosio::abi/1.0"
})";

template<typename T>
void test_int(eosio::abi& abi1, eosio::abi& abi2) {
   for(T i : {T(0), T(1), std::numeric_limits<T>::min(), std::numeric_limits<T>::max()}) {
      test(i, abi1, abi2);
   }
}

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
//using abieos::key_type;
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

int main() {
   eosio::json_token_stream stream(empty_abi);
   eosio::abi_def def = eosio::from_json<eosio::abi_def>(stream).value();
   eosio::abi abi;
   CHECK(convert(def, abi));
   CHECK(abi.add_type<struct_type>());
   eosio::abi new_abi(round_trip_abi(abi));
   test(true, abi, new_abi);
   test(false, abi, new_abi);
   for(int i = -128; i <= 127; ++i) {
      test(static_cast<std::int8_t>(i), abi, new_abi);
   }
   for(int i = 0; i <= 255; ++i) {
      test(static_cast<std::uint8_t>(i), abi, new_abi);
   }
   for(int i = -32768; i <= 32767; ++i) {
      test(static_cast<std::int16_t>(i), abi, new_abi);
   }
   for(int i = 0; i <= 65535; ++i) {
      test(static_cast<std::uint16_t>(i), abi, new_abi);
   }
   test_int<int32_t>(abi, new_abi);
   test_int<uint32_t>(abi, new_abi);
   test_int<int64_t>(abi, new_abi);
   test_int<uint64_t>(abi, new_abi);
   test(int128{}, abi, new_abi);
   test(int128{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, abi, new_abi);
   test(int128{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, abi, new_abi);
   test(int128{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F}, abi, new_abi);
   test(int128{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80}, abi, new_abi);
   test(uint128{}, abi, new_abi);
   test(uint128{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, abi, new_abi);
   test(uint128{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, abi, new_abi);
   test(uint128{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F}, abi, new_abi);
   test(uint128{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80}, abi, new_abi);
   test(varuint32{0}, abi, new_abi);
   test(varuint32{1}, abi, new_abi);
   test(varuint32{0xFFFFFFFFu}, abi, new_abi);
   test(varint32{0}, abi, new_abi);
   test(varint32{1}, abi, new_abi);
   test(varint32{-1}, abi, new_abi);
   test(varint32{0x7FFFFFFF}, abi, new_abi);
   test(varint32{std::numeric_limits<int32_t>::min()}, abi, new_abi);
   test(0.0f, abi, new_abi);
   test(1.0f, abi, new_abi);
   test(-1.0f, abi, new_abi);
   test(std::numeric_limits<float>::min(), abi, new_abi);
   test(std::numeric_limits<float>::max(), abi, new_abi);
   test(std::numeric_limits<float>::infinity(), abi, new_abi);
   test(-std::numeric_limits<float>::infinity(), abi, new_abi);
   // nans are not equal
   // test(std::numeric_limits<float>::quiet_NaN(), abi, new_abi);
   test(0.0, abi, new_abi);
   test(1.0, abi, new_abi);
   test(-1.0, abi, new_abi);
   test(std::numeric_limits<double>::min(), abi, new_abi);
   test(std::numeric_limits<double>::max(), abi, new_abi);
   test(std::numeric_limits<double>::infinity(), abi, new_abi);
   test(-std::numeric_limits<double>::infinity(), abi, new_abi);
   test(float128{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, abi, new_abi);
   test(float128{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80}, abi, new_abi);
   test(float128{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, abi, new_abi);
   for(uint64_t i = 0; i < 10000; ++i) {
      test(time_point{i * 1000}, abi, new_abi);
   }
   // This is the largest time that can be parsed by the current implementation,
   // because of the dependency on time_point_sec.
   test(time_point{0xFFFFFFFFull * 1000000}, abi, new_abi);
   for(uint32_t i = 0; i < 10000; ++i) {
      test(time_point_sec{i}, abi, new_abi);
   }
   test(time_point_sec{0xFFFFFFFFu}, abi, new_abi);
   for(uint32_t i = 0; i < 10000; ++i) {
      test(block_timestamp{i}, abi, new_abi);
   }
   test(block_timestamp{0xFFFFFFFFu}, abi, new_abi);
   test(abieos::name("eosio"), abi, new_abi);
   test(abieos::name(), abi, new_abi);
   test(bytes(), abi, new_abi);
   test(bytes{{0, 0, 0, 0}}, abi, new_abi);
   test(bytes{{'\xff', '\xff', '\xff', '\xff'}}, abi, new_abi);
   using namespace std::literals::string_literals;
   test(""s, abi, new_abi);
   test("\0"s, abi, new_abi);
   // test("\xff"s, abi, new_abi); // invalid utf8 doesn't round-trip
   test("abcdefghijklmnopqrstuvwxyz"s, abi, new_abi);
   test(checksum160{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, abi, new_abi);
   test(checksum256{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, abi, new_abi);
   test(checksum512{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, abi, new_abi);
   test(public_key{std::in_place_index<0>}, abi, new_abi);
   test(public_key{std::in_place_index<1>}, abi, new_abi);
   test(private_key{std::in_place_index<0>}, abi, new_abi);
   test(private_key{std::in_place_index<1>}, abi, new_abi);
   test(signature{std::in_place_index<0>}, abi, new_abi);
   test(signature{std::in_place_index<1>}, abi, new_abi);
   test(symbol{unsigned('ZYX\x08')}, abi, new_abi);
   test(symbol_code{unsigned('ZYXW')}, abi, new_abi);
   test(asset{5, symbol{'ZYX\x08'}}, abi, new_abi);
   test(struct_type{}, abi, new_abi);
   test(struct_type{{1},2,3}, abi, new_abi);
   test(struct_type{{1,2},3,4.0}, abi, new_abi);
   test(std::vector{1, 2}, abi, new_abi);
   test(std::optional{3}, abi, new_abi);
   test(std::variant<int, double>{4}, abi, new_abi);
   if(error_count) return 1;
}
