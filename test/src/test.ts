// copyright defined in abieos/LICENSE.txt

'use strict';

const rpcEndpoint = 'http://localhost:8000';
const useRpcEndpoint = false;

const fetch = require('node-fetch');
const fastcall = require('fastcall');
const transactionAbi = require('json-loader!../../external/eosjs2/src/transaction.abi');
import * as eosjs2 from '../../external/eosjs2/src/index';
import * as eosjs2_jsonrpc from '../../external/eosjs2/src/eosjs2-jsonrpc';
import * as eosjs2_jssig from '../../external/eosjs2/src/eosjs2-jssig';

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
    .function('char* abieos_get_type_for_action(void* context, uint64 contract, uint64 action)')
    .function('int abieos_json_to_bin(void* context, uint64 contract, char* name, char* json)')
    .function('char* abieos_hex_to_json(void* context, uint64 contract, char* type, char* hex)');

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
    return result;
}

function jsonStr(v: any) {
    return cstr(JSON.stringify(v));
}

function name(s: string) {
    return l.abieos_string_to_name(context, cstr(s));
}

const rpc = new eosjs2_jsonrpc.JsonRpc(rpcEndpoint, { fetch });
const signatureProvider = new eosjs2_jssig.default(['5JtUScZK2XEp3g9gh7F8bwtPTRAkASmNrrftmx4AxDKD5K4zDnr']);
const api = new eosjs2.Api({ rpc, signatureProvider, chainId: null });
const js2Types = eosjs2.serialize.getTypesFromAbi(eosjs2.serialize.createInitialTypes(), transactionAbi);

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
    let hex = json_to_hex(0, type, data);
    let json = hex_to_json(0, type, hex);
    console.log(type, data, hex, json);
    if (json !== expected)
        throw new Error('conversion mismatch');
    json = JSON.stringify(JSON.parse(json));

    //console.log(type, data);
    let js2Type = eosjs2.serialize.getType(js2Types, type);
    let buf = new eosjs2.serialize.SerialBuffer({ textEncoder: new (require('util').TextEncoder), textDecoder: new (require('util').TextDecoder)('utf-8', { fatal: true }) });
    js2Type.serialize(buf, JSON.parse(data));
    let js2Hex = eosjs2.serialize.arrayToHex(buf.asUint8Array()).toUpperCase();
    //console.log(hex)
    //console.log(js2Hex)
    if (js2Hex != hex)
        throw new Error('eosjs2 hex mismatch');
    let js2Json = JSON.stringify(js2Type.deserialize(buf));
    //console.log(json);
    //console.log(js2Json);
    if (js2Json != json)
        throw new Error('eosjs2 json mismatch');
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
    check_type('int64', '0', '"0"');
    check_type('int64', '1', '"1"');
    check_type('int64', '-1', '"-1"');
    check_type('int64', '"0"');
    check_type('int64', '"9223372036854775807"');
    check_type('int64', '"-9223372036854775808"');
    check_type('uint64', '"0"');
    check_type('uint64', '"18446744073709551615"');
    check_type('int128', '"0"');
    check_type('int128', '"1"');
    check_type('int128', '"-1"');
    check_type('int128', '"18446744073709551615"');
    check_type('int128', '"-18446744073709551615"');
    check_type('int128', '"170141183460469231731687303715884105727"');
    check_type('int128', '"-170141183460469231731687303715884105727"');
    check_type('int128', '"-170141183460469231731687303715884105728"');
    check_type('uint128', '"0"');
    check_type('uint128', '"1"');
    check_type('uint128', '"18446744073709551615"');
    check_type('uint128', '"340282366920938463463374607431768211454"');
    check_type('uint128', '"340282366920938463463374607431768211455"');
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
    check_type('varint32', '0');
    check_type('varint32', '-1');
    check_type('varint32', '1');
    check_type('varint32', '-2');
    check_type('varint32', '2');
    check_type('varint32', '-2147483647');
    check_type('varint32', '2147483647');
    check_type('varint32', '-2147483648');
    check_type('float32', '0.0');
    check_type('float32', '0.125');
    check_type('float32', '-0.125');
    check_type('float64', '0.0');
    check_type('float64', '0.125');
    check_type('float64', '-0.125');
    check_type('float128', '"00000000000000000000000000000000"');
    check_type('float128', '"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"');
    check_type('float128', '"12345678ABCDEF12345678ABCDEF1234"');
    check_type('time_point_sec', '"1970-01-01T00:00:00.000"');
    check_type('time_point_sec', '"2018-06-15T19:17:47.000"');
    check_type('time_point_sec', '"2060-06-15T19:17:47.000"');
    check_type('time_point', '"1970-01-01T00:00:00.000"');
    check_type('time_point', '"1970-01-01T00:00:00.001"');
    check_type('time_point', '"1970-01-01T00:00:00.002"');
    check_type('time_point', '"1970-01-01T00:00:00.010"');
    check_type('time_point', '"1970-01-01T00:00:00.100"');
    check_type('time_point', '"2018-06-15T19:17:47.000"');
    check_type('time_point', '"2018-06-15T19:17:47.999"');
    check_type('time_point', '"2060-06-15T19:17:47.999"');
    check_type('block_timestamp_type', '"2000-01-01T00:00:00.000"');
    check_type('block_timestamp_type', '"2000-01-01T00:00:00.500"');
    check_type('block_timestamp_type', '"2000-01-01T00:00:01.000"');
    check_type('block_timestamp_type', '"2018-06-15T19:17:47.500"');
    check_type('block_timestamp_type', '"2018-06-15T19:17:48.000"');
    check_type('name', '""', '"............."');
    check_type('name', '"1"');
    check_type('name', '"abcd"');
    check_type('name', '"ab.cd.ef"');
    check_type('name', '"ab.cd.ef.1234"');
    check_type('name', '"..ab.cd.ef.."', '"..ab.cd.ef"');
    check_type('name', '"zzzzzzzzzzzz"');
    check_type('name', '"zzzzzzzzzzzzz"', '"zzzzzzzzzzzzj"');
    check_type('bytes', '""');
    check_type('bytes', '"00"');
    check_type('bytes', '"AABBCCDDEEFF00010203040506070809"');
    check_type('string', '""');
    check_type('string', '"z"');
    check_type('string', '"This is a string."');
    check_type('string', '"' + '*'.repeat(128) + '"');
    check_type('checksum160', '"0000000000000000000000000000000000000000"');
    check_type('checksum160', '"123456789ABCDEF01234567890ABCDEF70123456"');
    check_type('checksum256', '"0000000000000000000000000000000000000000000000000000000000000000"');
    check_type('checksum256', '"0987654321ABCDEF0987654321FFFF1234567890ABCDEF001234567890ABCDEF"');
    check_type('checksum512', '"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"');
    check_type('checksum512', '"0987654321ABCDEF0987654321FFFF1234567890ABCDEF001234567890ABCDEF0987654321ABCDEF0987654321FFFF1234567890ABCDEF001234567890ABCDEF"');
    check_type('public_key', '"EOS1111111111111111111111111111111114T1Anm"')
    check_type('public_key', '"EOS11111111111111111111111115qCHTcgbQwptSz99m"')
    check_type('public_key', '"EOS111111111111111114ZrjxJnU1LA5xSyrWMNuXTrYSJ57"')
    check_type('public_key', '"EOS1111111113diW7pnisfdBvHTXP7wvW5k5Ky1e5DVuF23dosU"')
    check_type('public_key', '"EOS11DsZ6Lyr1aXpm9aBqqgV4iFJpNbSw5eE9LLTwNAxqjJgmjgbT"')
    check_type('public_key', '"EOS12wkBET2rRgE8pahuaczxKbmv7ciehqsne57F9gtzf1PVYNMRa2"')
    check_type('public_key', '"EOS1yp8ebBuKZ13orqUrZsGsP49e6K3ThVK1nLutxSyU5j9SaXz9a"')
    check_type('public_key', '"EOS9adaAMuB9v8yX1mZ5PtoB6VFSCeqRGjASd8ZTM6VUkiHL7mue4K"')
    check_type('public_key', '"EOS69X3383RzBZj41k73CSjUNXM5MYGpnDxyPnWUKPEtYQmTBWz4D"')
    check_type('public_key', '"EOS7yBtksm8Kkg85r4in4uCbfN77uRwe82apM8jjbhFVDgEgz3w8S"')
    check_type('public_key', '"EOS7WnhaKwHpbSidYuh2DF1qAExTRUtPEdZCaZqt75cKcixuQUtdA"')
    check_type('public_key', '"EOS7Bn1YDeZ18w2N9DU4KAJxZDt6hk3L7eUwFRAc1hb5bp6xJwxNV"')
    check_type('public_key', '"PUB_R1_1111111111111111111111111111111116amPNj"')
    check_type('public_key', '"PUB_R1_67vQGPDMCR4gbqYV3hkfNz3BfzRmmSj27kFDKrwDbaZKtaX36u"')
    check_type('public_key', '"PUB_R1_6FPFZqw5ahYrR9jD96yDbbDNTdKtNqRbze6oTDLntrsANgQKZu"')
    check_type('public_key', '"PUB_R1_7zetsBPJwGQqgmhVjviZUfoBMktHinmTqtLczbQqrBjhaBgi6x"')
    check_type('private_key', '"PVT_R1_PtoxLPzJZURZmPS4e26pjBiAn41mkkLPrET5qHnwDvbvqFEL6"');
    check_type('private_key', '"PVT_R1_vbRKUuE34hjMVQiePj2FEjM8FvuG7yemzQsmzx89kPS9J8Coz"');
    check_type('signature', '"SIG_K1_Kg2UKjXTX48gw2wWH4zmsZmWu3yarcfC21Bd9JPj7QoDURqiAacCHmtExPk3syPb2tFLsp1R4ttXLXgr7FYgDvKPC5RCkx"');
    check_type('signature', '"SIG_R1_Kfh19CfEcQ6pxkMBz6xe9mtqKuPooaoyatPYWtwXbtwHUHU8YLzxPGvZhkqgnp82J41e9R6r5mcpnxy1wAf1w9Vyo9wybZ"');
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
    check_type('asset[]', '[]')
    check_type('asset[]', '["0 FOO"]')
    check_type('asset[]', '["0 FOO","0.000 FOO"]')
    check_type('asset?', 'null')
    check_type('asset?', '"0.123456 SIX"')
    check_type('extended_asset', '{"quantity":"0 FOO","contract":"bar"}')
    check_type('extended_asset', '{"quantity":"0.123456 SIX","contract":"seven"}')
}

async function push_transfer() {
    check(l.abieos_set_abi(context, name('eosio.token'), jsonStr((await rpc.get_abi('eosio.token')).abi)));
    let type = checkPtr(l.abieos_get_type_for_action(context, name('eosio.token'), name('transfer')));
    check(l.abieos_json_to_bin(context, name('eosio.token'), type, jsonStr({
        from: 'useraaaaaaaa',
        to: 'useraaaaaaab',
        quantity: '0.0001 SYS',
        memo: '',
    })));
    const actionDataHex = l.abieos_get_bin_hex(context).readCString();
    console.log('action json->bin: ', actionDataHex);
    console.log('action bin->json: ', hex_to_json(name('eosio.token'), 'transfer', actionDataHex));

    let info = await rpc.get_info();
    let refBlock = await rpc.get_block(info.head_block_num - 3);
    let transaction = {
        expiration: eosjs2.serialize.timePointSecToDate(eosjs2.serialize.dateToTimePointSec(refBlock.timestamp) + 10),
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
    console.log('transaction json->bin: ', transactionDataHex);
    console.log('transaction bin->json: ', hex_to_json(0, 'transaction', transactionDataHex));

    let sig = await signatureProvider.sign({ chainId: info.chain_id, serializedTransaction: eosjs2.serialize.hexToUint8Array(transactionDataHex) });
    console.log('sig:', sig)

    let result = await rpc.fetch('/v1/chain/push_transaction', {
        signatures: sig,
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
        check_types();
        if (useRpcEndpoint)
            await push_transfer();
    } catch (e) {
        console.log(e);
    }
})();
