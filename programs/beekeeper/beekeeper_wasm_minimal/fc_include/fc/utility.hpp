#pragma once
// Minimal FC utility stub for WASM build

#include <cstdint>
#include <cstddef>
#include <utility>

namespace fc {
    using std::size_t;

    template<typename T>
    typename std::remove_reference<T>::type&& move(T&& t) {
        return static_cast<typename std::remove_reference<T>::type&&>(t);
    }
}
