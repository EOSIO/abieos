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
    .function('int abieos_json_to_bin(void* context, uint64 contract, char* name, char* json)')
    .function('char* abieos_hex_to_json(void* context, uint64 contract, char* type, char* hex)');

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

function checkPtr(result: any) {
    if (result.isNull())
        throw new Error(l.abieos_get_error(context).readCString());
}

function jsonStr(v: any) {
    return cstr(JSON.stringify(v));
}

function name(s: string) {
    return l.abieos_string_to_name(context, cstr(s));
}

const rpc = new eosjs2_jsonrpc.JsonRpc('http://localhost:8000', { fetch });
const signatureProvider = new eosjs2_jssig.default(['5JtUScZK2XEp3g9gh7F8bwtPTRAkASmNrrftmx4AxDKD5K4zDnr']);
const api = new eosjs2.Api({ rpc, signatureProvider, chainId: null });

function json_to_hex(contract: number, type: string, data: string) {
    check(l.abieos_json_to_bin(context, contract, cstr(type), cstr(data)));
    return l.abieos_get_bin_hex(context).readCString();
}

function hex_to_json(contract: number, type: string, hex: string) {
    let result = l.abieos_hex_to_json(context, contract, cstr(type), cstr(hex));
    checkPtr(result);
    return result.readCString();
}

function check_type(type: string, data: string, expected = data) {
    //console.log('a', data);
    let hex = json_to_hex(0, type, data);
    //console.log('b', hex);
    let final = hex_to_json(0, type, hex);
    console.log(type, data, hex, final);
    if (final !== expected)
        throw new Error('conversion mismatch');
}

function check_types() {
    check_type('bool', 'true');
    check_type('bool', 'false');
    check_type('int8', '0');
    check_type('int8', '127');
    check_type('int8', '-128');
    check_type('uint8', '0');
    check_type('uint8', '1');
    check_type('uint8', '254');
    check_type('uint8', '255');
    check_type('int16', '0');
    check_type('int16', '32767');
    check_type('int16', '-32768');
    check_type('uint16', '0');
    check_type('uint16', '65535');
    check_type('int32', '0');
    check_type('int32', '2147483647');
    check_type('int32', '-2147483648');
    check_type('uint32', '0');
    check_type('uint32', '4294967295');
    // int64
    // uint64
    // int128
    // uint128
    check_type('varuint32', '0');
    check_type('varuint32', '127');
    check_type('varuint32', '128');
    check_type('varuint32', '129');
    check_type('varuint32', '16383');
    check_type('varuint32', '16384');
    check_type('varuint32', '16385');
    check_type('varuint32', '2097151');
    check_type('varuint32', '2097152');
    check_type('varuint32', '2097153');
    check_type('varuint32', '268435455');
    check_type('varuint32', '268435456');
    check_type('varuint32', '268435457');
    check_type('varuint32', '4294967294');
    check_type('varuint32', '4294967295');
    check_type('float32', '0.0');
    check_type('float32', '0.125');
    check_type('float32', '-0.125');
    check_type('float64', '0.0');
    check_type('float64', '0.125');
    check_type('float64', '-0.125');
    // float128
    // time_point
    check_type('time_point_sec', '"1970-01-01T00:00:00.000"');
    check_type('time_point_sec', '"2018-06-15T19:17:47.000"');
    // block_timestamp_type
    check_type('name', '""', '"............."');
    check_type('name', '"1"');
    check_type('name', '"abcd"');
    check_type('name', '"ab.cd.ef"');
    check_type('name', '"ab.cd.ef.1234"');
    check_type('name', '"zzzzzzzzzzzz"');
    check_type('name', '"zzzzzzzzzzzzz"', '"zzzzzzzzzzzzj"');
    check_type('bytes', '""');
    check_type('bytes', '"00"');
    check_type('bytes', '"AABBCCDDEEFF00010203040506070809"');
    check_type('string', '""');
    check_type('string', '"z"');
    check_type('string', '"This is a string."');
    check_type('string', '"' + '*'.repeat(128) + '"');
    // checksum160
    // checksum256
    // checksum512
    // public_key
    // signature
    check_type('symbol_code', '"A"')
    check_type('symbol_code', '"B"')
    check_type('symbol_code', '"SYS"')
    check_type('symbol', '"0,A"')
    check_type('symbol', '"1,Z"')
    check_type('symbol', '"4,SYS"')
    check_type('asset', '"0 FOO"')
    check_type('asset', '"0.0 FOO"')
    check_type('asset', '"0.00 FOO"')
    check_type('asset', '"0.000 FOO"')
    check_type('asset', '"1.2345 SYS"')
    check_type('asset', '"-1.2345 SYS"')
    // extended_asset
}

async function push_transfer() {
    check(l.abieos_json_to_bin(context, name('eosio.token'), cstr('transfer'), jsonStr({
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
    check(l.abieos_json_to_bin(context, 0, cstr('transaction'), jsonStr(transaction)));
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
}

(async () => {
    try {
        check(context);
        check(l.abieos_set_abi(context, 0, jsonStr(transactionAbi)));
        check(l.abieos_set_abi(context, name('eosio.token'), jsonStr((await rpc.get_abi('eosio.token')).abi)));
        check_types();
        await push_transfer();
    } catch (e) {
        console.log(e);
    }
})();
