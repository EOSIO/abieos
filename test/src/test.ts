'use strict';

const fastcall = require('fastcall');
const transactionAbi = require('json-loader!../../external/eosjs2/src/transaction.abi');
import * as eosjs2 from '../../external/eosjs2/src/index';

const lib = new fastcall.Library('../build/libabieos.so')
.function('void* abieos_create()')
.function('void abieos_destroy(void* context)')
.function('char* abieos_get_error(void* context)')
.function('uint64 abieos_string_to_name(void* context, char* str)')
.function('char* abieos_name_to_string(void* context, uint64 name)')
.function('int abieos_set_abi(void* context, uint64 contract, char* abi)');

let {abieos_create,abieos_destroy,abieos_get_error,abieos_string_to_name,abieos_name_to_string,abieos_set_abi} = lib.interface;

const context = abieos_create();

function check(result:any) {
    if(!result)
        throw new Error(abieos_get_error(context).readCString());
}

check(context);
check(abieos_set_abi(context, 0, fastcall.makeStringBuffer(JSON.stringify(transactionAbi))));
