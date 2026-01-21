#pragma once
// Minimal FC string stub for WASM build
// Simply aliases fc::string to std::string

#include <string>

namespace fc {
    typedef std::string string;
}
