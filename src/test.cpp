// copyright defined in abieos/LICENSE.txt

#include "abieos.hpp"
#include <stdio.h>

using namespace abieos;

const char json[] = R"({
    "foo": "bar"
})";

int main() {
    abi_def x{};
    // bin_to_native(x);
    json_to_native(x, json);
}
