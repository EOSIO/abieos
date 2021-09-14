#include <eosio/ship_protocol.hpp>
#include <eosio/abi.hpp>
#include <iostream>
#include <regex>
#include <iterator>

namespace eosio {
namespace ship_protocol {

using types = std::tuple<request, result, account, account_metadata, code, contract_index_double, contract_index_long_double, contract_index128,
               contract_index256, contract_index64, contract_row, contract_table, generated_transaction,
               global_property, key_value, permission, permission_link, protocol_state, resource_limits,
               resource_limits_config, resource_limits_state, resource_usage>;
} // namespace ship_protocol

inline abi_type* add_type(abi& a, ship_protocol::transaction_status*) {
    return &a.abi_types.find("uint8")->second;
}

inline abi_type* add_type(abi& a, std::vector<ship_protocol::recurse_transaction_trace>*) {
    return a.add_type<std::optional<ship_protocol::transaction_trace>>();
}
} // namespace eosio

int main() { 
   
   eosio::abi abi;
   eosio::abi_def empty_def;
   eosio::convert(empty_def, abi);

   std::apply([&abi](auto... x) { 
      (abi.add_type<std::decay_t<decltype(x)>>(),...); 
   }, eosio::ship_protocol::types{});

   eosio::abi_def ship_abi_def;
   eosio::convert(abi, ship_abi_def);

   using namespace eosio::literals;

   ship_abi_def.tables = {
       eosio::table_def{.name = "account"_n, .key_names = {"name"}, .type = "account"},
       eosio::table_def{.name = "actmetadata"_n, .key_names = {"name"}, .type = "account_metadata"},
       eosio::table_def{.name = "code"_n, .key_names = {"vm_type", "vm_version", "code_hash"}, .type = "code"},
       eosio::table_def{.name = "contracttbl"_n, .key_names = {"code", "scope", "table"}, .type = "contract_table"},
       eosio::table_def{.name = "contractrow"_n, .key_names = {"code", "scope", "table", "primary_key"}, .type = "contract_row"},
       eosio::table_def{.name = "cntrctidx1"_n, .key_names = {"code", "scope", "table", "primary_key"}, .type = "contract_index64"},
       eosio::table_def{.name = "cntrctidx2"_n, .key_names = {"code", "scope", "table", "primary_key"}, .type = "contract_index128"},
       eosio::table_def{.name = "cntrctidx3"_n, .key_names = {"code", "scope", "table", "primary_key"}, .type = "contract_index256"},
       eosio::table_def{.name = "cntrctidx4"_n,
        .key_names = {"code", "scope", "table", "primary_key"},
        .type = "contract_index_double"},
       eosio::table_def{.name = "cntrctidx5"_n,
        .key_names = {"code", "scope", "table", "primary_key"},
        .type = "contract_index_long_double"},
       eosio::table_def{.name = "keyvalue"_n, .key_names = {"contract", "key"}, .type = "key_value"},
       eosio::table_def{.name = "global.pty"_n, .key_names = {}, .type = "global_property"},
       eosio::table_def{.name = "generatedtrx"_n, .key_names = {"sender", "sender_id"}, .type = "generated_transaction"},
       eosio::table_def{.name = "protocolst"_n, .key_names = {}, .type = "protocol_state"},
       eosio::table_def{.name = "permission"_n, .key_names = {"owner", "name"}, .type = "permission"},
       eosio::table_def{.name = "permlink"_n, .key_names = {"account", "code", "message_type"}, .type = "permission_link"},
       eosio::table_def{.name = "rsclimits"_n, .key_names = {"owner"}, .type = "resource_limits"},
       eosio::table_def{.name = "rscusage"_n, .key_names = {"owner"}, .type = "resource_usage"},
       eosio::table_def{.name = "rsclimitsst"_n, .key_names = {}, .type = "resource_limits_state"},
       eosio::table_def{.name = "rsclimitscfg"_n, .key_names = {}, .type = "resource_limits_config"}};

   std::vector<char> data;
   eosio::vector_stream strm{data};
   to_json(ship_abi_def, strm);

   // remove the empty value elements in the json, like {"name": "myname","type":""}  ==> {"name":"myname"};
   // however, do not remove empty `fields` elements, like {'name': 'get_status_request_v0', "fields":[] } ==> {'name': 'get_status_request_v0', "fields":[] }
   std::regex empty_value_re(R"===(,"(?!fields\b)\b[^"]+":(""|\[\]|\{\}))===");
   std::regex_replace(std::ostreambuf_iterator<char>(std::cout),
                      data.begin(), data.end(), empty_value_re, "");
   return 0;
}
