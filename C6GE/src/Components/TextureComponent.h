#pragma once
#include <string>
#include <bgfx/bgfx.h>

class TextureComponent {
public:
    std::string diffusePath;
    bgfx::TextureHandle diffuse = BGFX_INVALID_HANDLE;
    std::string roughnessPath;
    bgfx::TextureHandle roughness = BGFX_INVALID_HANDLE;
    bool loaded = false;

    TextureComponent() = default;
    explicit TextureComponent(const std::string& path) : diffusePath(path) {}

    bool Load();
    void Cleanup();
};