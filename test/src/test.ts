'use strict';

const endpoint = 'http://localhost:8000';

const fetch = require('node-fetch');


const fastcall = require('fastcall');
const transactionAbi = require('json-loader!../../external/eosjs2/src/transaction.abi');
//import * as eosjs2 from '../../external/eosjs2/src/index';
import { JsonRpc } from '../../external/eosjs2/src/eosjs2-jsonrpc';

const lib = new fastcall.Library('../build/libabieos.so')
    .function('void* abieos_create()')
    .function('void abieos_destroy(void* context)')
    .function('char* abieos_get_error(void* context)')
    .function('int abieos_get_bin_size(void* context)')
    .function('char* abieos_get_bin_data(void* context)')
    .function('uint64 abieos_string_to_name(void* context, char* str)')
    .function('char* abieos_name_to_string(void* context, uint64 name)')
    .function('int abieos_set_abi(void* context, uint64 contract, char* abi)')
    .function('int abieos_json_to_bin_struct(void* context, uint64 contract, char* name, char* json)');

let l = lib.interface;
let cstr = fastcall.makeStringBuffer;

const context = l.abieos_create();

function check(result: any) {
    if (!result)
        throw new Error(l.abieos_get_error(context).readCString());
}

function jsonStr(v: any) {
    return cstr(JSON.stringify(v));
}

function name(s: string) {
    return l.abieos_string_to_name(context, cstr(s));
}

(async () => {
    try {
        const rpc = new JsonRpc(endpoint, { fetch });
        check(context);
        check(l.abieos_set_abi(context, 0, jsonStr(transactionAbi)));
        check(l.abieos_set_abi(context, name('eosio.token'), jsonStr((await rpc.get_abi('eosio.token')).abi)));
        check(l.abieos_json_to_bin_struct(context, name('eosio.token'), cstr('transfer'), jsonStr({})));
    } catch (e) {
        console.log(e);
    }
})();
