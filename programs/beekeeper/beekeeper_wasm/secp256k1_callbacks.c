/// Lightweight secp256k1 default callbacks for WASM builds.
/// Replaces the default implementations that use fprintf (pulling in ~6 KB of printf machinery).
/// These simply abort — the error messages are only useful for debugging secp256k1 itself.

#include <stdlib.h>

void secp256k1_default_illegal_callback_fn(const char* str, void* data) {
    (void)str;
    (void)data;
    abort();
}

void secp256k1_default_error_callback_fn(const char* str, void* data) {
    (void)str;
    (void)data;
    abort();
}
