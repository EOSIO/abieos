## abieos

Binary <> JSON conversion using ABIs. Compatible with languages which can interface to C; see [src/abieos.h](src/abieos.h).

## Packing transactions

1. Create a context: `abieos_create`
1. Use `abieos_set_abi` to load [external/eosjs2/src/transaction.abi](external/eosjs2/src/transaction.abi) into contract 0.
1. Use `abieos_set_abi` to load the contract's ABI.
1. Use `abieos_json_to_bin` and `abieos_get_bin_hex` to convert action data to hex. Use `abieos_get_type_for_action` to get the action's type.
1. Use `abieos_json_to_bin` and `abieos_get_bin_hex` to convert transaction to hex. Use `contract = 0` and `type = abieos_string_to_name(context, "transaction")`.
1. Destroy the context: `abieos_destroy`

## Usage note

abieos expects object attributes to be in order. It will complain about missing attributes if they are out of order.

## Example data

Example action data for `abieos_json_to_bin`:

```
{
    "from": "useraaaaaaaa",
    "to": "useraaaaaaab",
    "quantity": "0.0001 SYS",
    "memo": ""
}
```

Example transaction data for `abieos_json_to_bin`:

```
{
    "expiration": "2018-06-27T20:33:54.000",
    "ref_block_num": 45323,
    "ref_block_prefix": 2628749070,
    "max_net_usage_words": 0,
    "max_cpu_usage_ms": 0,
    "delay_sec": 0,
    "context_free_actions": [],
    "actions": [{
        "account": "eosio.token",
        "name": "transfer",
        "authorization":[{
            "actor":"useraaaaaaaa",
            "permission":"active"
        }],
        "data":"608C31C6187315D6708C31C6187315D60100000000000000045359530000000000"
    }],
    "transaction_extensions":[]
}
```
