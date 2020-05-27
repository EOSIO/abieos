#include "../src/abieos.h"
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

void usage(char* name, bool reverse) {
    if (reverse) {
        fprintf(stderr, "Usage: %s abi_file.json type json\n", name);
        fputs("This example takes a json value, and returns the corresponding abieos encoded hex string\n", stderr);
        fputs("Example:\n", stderr);
        fprintf(stderr, "$ ./%s transaction.abi.json transaction '{your json here}'\n", name);
    } else {
        fprintf(stderr, "Usage: %s abi_file.json hex-type hex-string\n", name);
        fputs("This example takes a abieos encoded hex string, and returns the corresponding json\n", stderr);
        fputs("Example:\n", stderr);
        fprintf(stderr,
                "$ ./%s transaction.abi.json transaction "
                "AE0D635CDCAC90A6DCFA000000000100A6823403EA3055000000572D3CCDCD0100AEAA4AC15CFD4500000000A8ED32323B00AE"
                "AA4AC15CFD4500000060D234CD3DA06806000000000004454F53000000001A746865206772617373686F70706572206C696573"
                "20686561767900\n",
                name);
    }
}

int main(int argc, char* argv[]) {
    const char* contract_str = "eosio";
    bool reverse = false;
    char* program_name = basename(argv[0]);

    if (strcasecmp(program_name, "json2hex") == 0) {
        reverse = true;
    }

    if (argc != 4) {
        usage(program_name, reverse);
        return 1;
    }
    char* abi_filename = argv[1];
    char* hex_type = argv[2];
    char* hex_json_string = argv[3];

    char* source = NULL;
    FILE* abi_file = fopen(abi_filename, "r");
    if (abi_file == NULL) {
        fprintf(stderr, "unable to open abi_file.json %s\n", abi_filename);
        return -1;
    }
    /* Go to the end of the file. */
    if (fseek(abi_file, 0L, SEEK_END) == 0) {
        /* Get the size of the file. */
        long bufsize = ftell(abi_file);
        if (bufsize == -1) {
            /* Error */
            fprintf(stderr, "ftell error\n");
            return -1;
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

    abieos_context* context = abieos_create();
    if (context == NULL) {
        fprintf(stderr, "Unable to create context\n");
        free(source);
        return -1;
    }

    uint64_t contract_name = abieos_string_to_name(context, contract_str);
    if (contract_name == 0) {
        fprintf(stderr, "Error: abieos_string_to_name %s\n", abieos_get_error(context));
    } else {
        abieos_bool result = abieos_set_abi(context, contract_name, source);
        if (!result) {
            fprintf(stderr, "Error: abieos_set_abi %s\n", abieos_get_error(context));
        } else {
            if (reverse) {
                abieos_bool j2b_result = abieos_json_to_bin(context, contract_name, hex_type, hex_json_string);
                if (j2b_result) {
                    const char* hex = abieos_get_bin_hex(context);
                    fprintf(stdout, "%s\n", hex);
                } else {
                    fprintf(stderr, "Error: %s %s\n", program_name, abieos_get_error(context));
                }

            } else {
                const char* json = abieos_hex_to_json(context, contract_name, hex_type, hex_json_string);
                if (!json) {
                    fprintf(stderr, "Error: %s %s\n", program_name, abieos_get_error(context));
                } else {
                    fprintf(stdout, "%s\n", json);
                }
            }
        }
    }
    abieos_destroy(context);
    free(source);

    return 0;
}
