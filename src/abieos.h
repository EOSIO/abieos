// copyright defined in eosjs2/LICENSE.txt

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct abieos_context_s abieos_context;
typedef int abieos_bool;

abieos_context* abieos_create();
void abieos_destroy(abieos_context* context);
const char* abieos_get_error(abieos_context* context);
int abieos_get_bin_size(abieos_context* context);
const char* abieos_get_bin_data(abieos_context* context);
const char* abieos_get_bin_hex(abieos_context* context);
uint64_t abieos_string_to_name(abieos_context* context, const char* str);
const char* abieos_name_to_string(abieos_context* context, uint64_t name);
abieos_bool abieos_set_abi(abieos_context* context, uint64_t contract, const char* abi);
const char* abieos_get_type_for_action(abieos_context* context, uint64_t contract, uint64_t action);
abieos_bool abieos_json_to_bin(abieos_context* context, uint64_t contract, const char* type, const char* json);
const char* abieos_bin_to_json(abieos_context* context, uint64_t contract, const char* type, const char* data,
                               size_t size);
const char* abieos_hex_to_json(abieos_context* context, uint64_t contract, const char* type, const char* hex);

#ifdef __cplusplus
}
#endif
