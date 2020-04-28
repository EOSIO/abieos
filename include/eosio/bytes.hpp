#pragma once

//#include <eosio/eosio_outcome.hpp>
#include <eosio/from_json.hpp>
#include <eosio/to_json.hpp>
#include <eosio/operators.hpp>
#include <vector>

namespace eosio {

struct bytes {
   std::vector<char> data;
};

EOSIO_REFLECT(bytes, data);
EOSIO_COMPARE(bytes);

template <typename S>
bool from_json(bytes& obj, S& stream, std::string_view& err) {
   return eosio::from_json_hex(obj.data, stream, err);
}

template <typename S>
bool to_json(const bytes& obj, S& stream, std::string_view& err) {
   return eosio::to_json_hex(obj.data.data(), obj.data.size(), stream, err);
}

} // namespace eosio
