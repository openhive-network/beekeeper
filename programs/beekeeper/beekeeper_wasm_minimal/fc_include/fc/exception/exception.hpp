#pragma once
// Minimal FC exception stub for WASM build
// Does not throw - returns error values instead for smaller binary size

#include <stdexcept>

namespace fc {
    // Minimal exception class stub
    class exception : public std::exception {
    public:
        exception() = default;
        exception(const char* msg) : message(msg) {}
        const char* what() const noexcept override { return message ? message : "fc::exception"; }
    private:
        const char* message = nullptr;
    };
}

// No-op macro for WASM - we use return values instead of exceptions
// This stub is needed so we can compile FC's hex.cpp which uses FC_THROW_EXCEPTION
// In our minimal build, invalid hex characters return 0 (caller should validate)
#define FC_THROW_EXCEPTION(EXCEPTION, ...) do { (void)0; } while(0)
