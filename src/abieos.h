#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct abieos_context_s abieos_context;

abieos_context* abieos_create();
void abieos_destroy(abieos_context* context);
const char* abieos_get_error(abieos_context* context);
uint64_t abieos_string_to_name(abieos_context* context, const char* str);
const char* abieos_name_to_string(abieos_context* context, uint64_t name);
int abieos_set_abi(abieos_context* context, uint64_t contract, const char* abi);

#ifdef __cplusplus
}
#endif
