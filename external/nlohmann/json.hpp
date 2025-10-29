#pragma once
// Single-header nlohmann/json v3.11.3 (minimal include)
// To keep the repo self-contained, we vendor the header. For full license and updates, see https://github.com/nlohmann/json
// This is a trimmed-forward include that assumes the full header is available in your environment. If not, replace with the official single-header.

#if __has_include(<nlohmann/json.hpp>)
#  include <nlohmann/json.hpp>
#else
// Fallback: embed a minimal shim that errors with a friendly message if not replaced
#include <stdexcept>
namespace nlohmann {
    struct json_shim_error : std::runtime_error { using std::runtime_error::runtime_error; };
    struct json {
        json() { throw json_shim_error("nlohmann/json.hpp not embedded. Replace external/nlohmann/json.hpp with the official single-header."); }
    };
}
#endif
