#include "abieos.h"
#include "abieos.hpp"

#include <memory>

using namespace abieos;

struct abieos_context_s {
    const char* last_error = "";
    std::string last_error_buffer{};
    std::string result_str{};

    std::map<name, contract> contracts{};
};

void set_error(abieos_context* context, const char* error) noexcept {
    try {
        context->last_error_buffer = error;
        context->last_error = context->last_error_buffer.c_str();
    } catch (...) {
        context->last_error = "exception while recording error";
    }
}

template <typename T, typename F>
auto handle_exceptions(abieos_context* context, T errval, F f) noexcept -> decltype(f()) {
    if (!context)
        return errval;
    try {
        return f();
    } catch (std::exception& e) {
        set_error(context, e.what());
        return errval;
    } catch (...) {
        set_error(context, "unknown exception");
        return errval;
    }
}

extern "C" abieos_context* abieos_create() {
    try {
        return new abieos_context{};
    } catch (...) {
        return nullptr;
    }
}

extern "C" void abieos_destroy(abieos_context* context) { delete context; }

extern "C" const char* abieos_get_error(abieos_context* context) {
    if (!context)
        return "context is null";
    return context->last_error;
}

extern "C" uint64_t abieos_string_to_name(abieos_context* context, const char* str) {
    if (!str)
        return 0;
    return string_to_name(str);
}

extern "C" const char* abieos_name_to_string(abieos_context* context, uint64_t name) {
    return handle_exceptions(context, nullptr, [&] {
        context->result_str = name_to_string(name);
        return context->result_str.c_str();
    });
}

extern "C" int abieos_set_abi(abieos_context* context, uint64_t contract, const char* abi) {
    return handle_exceptions(context, false, [&] {
        abi_def def{};
        json_to_native(def, abi);
        auto c = create_contract(def);
        context->contracts.insert({name{contract}, std::move(c)});
        return true;
    });
}
