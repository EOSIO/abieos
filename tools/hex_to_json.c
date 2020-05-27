//
// Created by Ian Holsman on 5/26/20.
//
#include "../src/abieos.h"
#include <stdio.h>
#include <stdlib.h>

void usage(char* name) {
    fprintf(stderr, "Usage: %s abi_file.json hex-type hex-string\n", name);
    fputs( "This example takes a abieos encoded hex string, and returns the corresponding json\n", stderr);
    fputs("Example:\n",stderr);
    fputs("$ ./hex_to_json transaction.abi.json transaction AE0D635CDCAC90A6DCFA000000000100A6823403EA3055000000572D3CCDCD0100AEAA4AC15CFD4500000000A8ED32323B00AEAA4AC15CFD4500000060D234CD3DA06806000000000004454F53000000001A746865206772617373686F70706572206C69657320686561767900\n",stderr);
}
int main(int argc, char* argv[]) {
    const char* contract_str = "eosio";
    if (argc != 4) {
        usage(argv[0]);
        return 1;
    }
    char* abi_filename = argv[1];
    char* hex_type = argv[2];
    char* hex_string = argv[3];

    char* source = NULL;
    FILE* abi_file = fopen(abi_filename, "r");
    if (abi_file != NULL) {
        /* Go to the end of the file. */
        if (fseek(abi_file, 0L, SEEK_END) == 0) {
            /* Get the size of the file. */
            long bufsize = ftell(abi_file);
            if (bufsize == -1) { /* Error */
            }

            /* Allocate our buffer to that size. */
            source = malloc(sizeof(char) * (bufsize + 1));

            /* Go back to the start of the file. */
            if (fseek(abi_file, 0L, SEEK_SET) != 0) { /* Error */
            }

            /* Read the entire file into memory. */
            size_t newLen = fread(source, sizeof(char), bufsize, abi_file);
            if (ferror(abi_file) != 0) {
                fputs("Error reading file\n", stderr);
            } else {
                source[newLen++] = '\0'; /* Just to be safe. */
            }
        }
        fclose(abi_file);
    } else {
        fprintf(stderr, "Unable to open abi_file.json %s\n", abi_filename);
        return -1;
    }
    abieos_context* context = abieos_create();
    if (context == NULL) {
        fprintf(stderr, "Unable to create context\n");
    } else {
        uint64_t contract_name = abieos_string_to_name(context, contract_str);
        if (contract_name == 0) {
            fprintf(stderr, "Error: abieos_string_to_name %s\n", abieos_get_error(context));
        } else {
            abieos_bool result = abieos_set_abi(context, contract_name, source);
            if (!result) {
                fprintf(stderr, "Error: abieos_set_abi %s\n", abieos_get_error(context));
            } else {
                const char* json = abieos_hex_to_json(context, contract_name, hex_type, hex_string);
                if (!json) {
                    fprintf(stderr, "Error: abieos_hex_to_json %s\n", abieos_get_error(context));

                } else {
                    fputs(json, stdout);
                }
            }
        }
        abieos_destroy(context);
    }

    free(source);

    return 0;
}
