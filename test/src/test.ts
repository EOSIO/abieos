'use strict';

const endpoint = 'http://localhost:8000';

const fetch = require('node-fetch');
const fastcall = require('fastcall');
const transactionAbi = require('json-loader!../../external/eosjs2/src/transaction.abi');
import * as eosjs2 from '../../external/eosjs2/src/index';
import * as eosjs2_jsonrpc from '../../external/eosjs2/src/eosjs2-jsonrpc';
import * as eosjs2_jssig from '../../external/eosjs2/src/eosjs2-jssig';
import * as eosjs2_ser from '../../external/eosjs2/src/eosjs2-serialize';

const lib = new fastcall.Library('../build/libabieos.so')
    .function('void* abieos_create()')
    .function('void abieos_destroy(void* context)')
    .function('char* abieos_get_error(void* context)')
    .function('int abieos_get_bin_size(void* context)')
    .function('char* abieos_get_bin_data(void* context)')
    .function('char* abieos_get_bin_hex(void* context)')
    .function('uint64 abieos_string_to_name(void* context, char* str)')
    .function('char* abieos_name_to_string(void* context, uint64 name)')
    .function('int abieos_set_abi(void* context, uint64 contract, char* abi)')
    .function('int abieos_json_to_bin_struct(void* context, uint64 contract, char* name, char* json)');

export function arrayToHex(data: Uint8Array) {
    let result = '';
    for (let x of data)
        result += ('00' + x.toString(16)).slice(-2);
    return result;
}

export function hexToUint8Array(hex: string) {
    let l = hex.length / 2;
    let result = new Uint8Array(l);
    for (let i = 0; i < l; ++i)
        result[i] = parseInt(hex.substr(i * 2, 2), 16);
    return result;
}

const l = lib.interface;
const cstr = fastcall.makeStringBuffer;

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
        const rpc = new eosjs2_jsonrpc.JsonRpc('http://localhost:8000', { fetch });
        const signatureProvider = new eosjs2_jssig.default(['5JtUScZK2XEp3g9gh7F8bwtPTRAkASmNrrftmx4AxDKD5K4zDnr']);
        const api = new eosjs2.Api({ rpc, signatureProvider, chainId: null });
        check(context);
        check(l.abieos_set_abi(context, 0, jsonStr(transactionAbi)));
        check(l.abieos_set_abi(context, name('eosio.token'), jsonStr((await rpc.get_abi('eosio.token')).abi)));
        check(l.abieos_json_to_bin_struct(context, name('eosio.token'), cstr('transfer'), jsonStr({
            from: 'useraaaaaaaa',
            to: 'useraaaaaaab',
            quantity: '0.0001 SYS',
            memo: '',
        })));
        const actionDataHex = l.abieos_get_bin_hex(context).readCString();
        console.log('abieos', actionDataHex);

        let info = await rpc.get_info();
        let refBlock = await rpc.get_block(info.head_block_num - 3);
        let transaction = {
            expiration: eosjs2_ser.timePointSecToDate(eosjs2_ser.dateToTimePointSec(refBlock.timestamp) + 10),
            ref_block_num: refBlock.block_num,
            ref_block_prefix: refBlock.ref_block_prefix,
            max_net_usage_words: 0,
            max_cpu_usage_ms: 0,
            delay_sec: 0,
            context_free_actions: [] as any,
            actions: [{
                account: 'eosio.token',
                name: 'transfer',
                authorization: [{
                    actor: 'useraaaaaaaa',
                    permission: 'active',
                }],
                data: actionDataHex,
            }],
            transaction_extensions: [] as any,
        };
        check(l.abieos_json_to_bin_struct(context, 0, cstr('transaction'), jsonStr(transaction)));
        let transactionDataHex = l.abieos_get_bin_hex(context).readCString();

        // let sig = await signatureProvider.sign({ chainId: info.chain_id, serializedTransaction: hexToUint8Array(transactionDataHex) });
        // console.log('sig:', sig)

        let result = await rpc.fetch('/v1/chain/push_transaction', {
            signatures: [],
            compression: 0,
            packed_context_free_data: '',
            packed_trx: transactionDataHex,
        });
        console.log(JSON.stringify(result, null, 4));
    } catch (e) {
        console.log(e);
    }
})();
