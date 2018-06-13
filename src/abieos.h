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
char* abieos_get_bin_data(abieos_context* context);
uint64_t abieos_string_to_name(abieos_context* context, const char* str);
const char* abieos_name_to_string(abieos_context* context, uint64_t name);
abieos_bool abieos_set_abi(abieos_context* context, uint64_t contract, const char* abi);
abieos_bool abieos_json_to_bin_struct(abieos_context* context, uint64_t contract, const char* name, const char* json);

#ifdef __cplusplus
}
#endif
