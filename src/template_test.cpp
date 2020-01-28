#include <eosio/to_json.hpp>
#include <eosio/from_json.hpp>
#include <eosio/to_bin.hpp>
#include <eosio/from_bin.hpp>
#include "abieos.hpp"

int error_count;

void report_error(const char* assertion, const char* file, int line) {
    printf("%s:%d: failed %s\n", file, line, assertion);
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

// Verifies that all 6 conversions between native/bin/json round-trip
template<typename T>
void test(const T& value, eosio::abi& abi) {
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

   {
      // Get the ABI
      const eosio::abi_type* type = abi.get_type(get_type_name((T*)nullptr)).value();

      // bin_to_json
      std::vector<char> bin2;
      CHECK(abieos::json_to_bin(bin2, type, {json.data(), json.size()}));
      CHECK(bin2 == bin);
      // json_to_bin
      eosio::input_stream bin_stream{bin};
      std::string json2;
      CHECK(abieos::bin_to_json(bin_stream, type, json2));
      CHECK(json2 == std::string(json.data(), json.size()));
   }
}

char empty_abi[] = R"({
    "version": "eosio::abi/1.0"
})";

int main() {
   eosio::json_token_stream stream(empty_abi);
   eosio::abi_def def = eosio::from_json<eosio::abi_def>(stream).value();
   eosio::abi abi;
   CHECK(convert(def, abi));
   test(abieos::name("eosio"), abi);
   test(abieos::name(), abi);
   if(error_count) return 1;
}
