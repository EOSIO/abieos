## abieos

Binary <> JSON conversion using ABIs. Compatible with languages which can interface to C; see [src/abieos.h](src/abieos.h).

Alpha release. Feedback requested.

## Packing transactions

1. Create a context: `abieos_create`
1. Use `abieos_set_abi` to load [eosjs2/src/transaction.abi](https://github.com/EOSIO/eosjs2/blob/master/src/transaction.abi) into contract 0.
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

## Ubuntu 16.04 with gcc 8.1.0

* Install these. You may have to build them yourself from source or find a PPA. Make them the default.
  * gcc 8.1.0
  * cmake 3.11.3
* `sudo apt install libboost-dev libboost-date-time-dev`
* remove this from CMakeLists.txt (2 places): `-fsanitize=address,undefined`

```
mkdir build
cd build
cmake ..
make
./test
```

## Contributing

[Contributing Guide](./CONTRIBUTING.md)

[Code of Conduct](./CONTRIBUTING.md#conduct)

## License

[MIT](./LICENSE)

## Important

See LICENSE for copyright and license terms.  Block.one makes its contribution on a voluntary basis as a member of the EOSIO community and is not responsible for ensuring the overall performance of the software or any related applications.  We make no representation, warranty, guarantee or undertaking in respect of the software or any related documentation, whether expressed or implied, including but not limited to the warranties or merchantability, fitness for a particular purpose and noninfringement. In no event shall we be liable for any claim, damages or other liability, whether in an action of contract, tort or otherwise, arising from, out of or in connection with the software or documentation or the use or other dealings in the software or documentation.  Any test results or performance figures are indicative and will not reflect performance under all conditions.  Any reference to any third party or third-party product, service or other resource is not an endorsement or recommendation by Block.one.  We are not responsible, and disclaim any and all responsibility and liability, for your use of or reliance on any of these resources. Third-party resources may be updated, changed or terminated at any time, so the information here may be out of date or inaccurate.
