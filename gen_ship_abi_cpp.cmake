file(READ "ship_abi.json" SHIP_ABI_JSON)
file(WRITE "ship_abi.cpp"  "namespace eosio { namespace ship_protocol { extern const char* const ship_abi = R\"(")
file(APPEND "ship_abi.cpp" "${SHIP_ABI_JSON}")
file(APPEND "ship_abi.cpp"  ")\"; }}")