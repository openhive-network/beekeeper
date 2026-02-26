// Stub out __cxa_demangle to prevent the full Itanium demangler (~48 KB, 257 functions)
// from being linked into the WASM binary. The demangler is only used by
// Emscripten's getExceptionMessage() to produce human-readable C++ type names
// in exception diagnostics. With this stub, exception.name will contain the
// mangled type name (e.g. "St13runtime_error") instead of the demangled one
// ("std::runtime_error"). Exception catching, what() messages, and all
// functional behavior are completely unaffected.

// This change saves ~60 KB of WASM binary size, which is significant for a 248 KB total.
// The demangler is not needed in production, and the mangled names are still somewhat useful for debugging.

extern "C" {

char* __cxa_demangle(const char* /*mangled_name*/, char* /*buf*/, unsigned long* /*length*/, int* status)
{
  if (status)
    *status = -2; // invalid mangled name — standard "I can't demangle this" return
  return nullptr;
}

} // extern "C"
