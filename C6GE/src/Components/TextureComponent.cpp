#include "TextureComponent.h"
#include <stb_image.h>

bool TextureComponent::Load() {
    if (loaded && bgfx::isValid(diffuse)) return true;

    int w = 0, h = 0, n = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* data = stbi_load(diffusePath.c_str(), &w, &h, &n, 4);
    if (!data) {
        return false;
    }

    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(w * h * 4));
    memcpy(mem->data, data, static_cast<size_t>(w * h * 4));
    diffuse = bgfx::createTexture2D((uint16_t)w, (uint16_t)h, false, 1, bgfx::TextureFormat::RGBA8, 0, mem);
    stbi_image_free(data);

    loaded = bgfx::isValid(diffuse);

    // Optional roughness
    if (!roughnessPath.empty()) {
        int rw = 0, rh = 0, rn = 0;
        unsigned char* rdata = stbi_load(roughnessPath.c_str(), &rw, &rh, &rn, 1);
        if (rdata) {
            // Expand to RGBA8 single channel into alpha or all channels
            const bgfx::Memory* rmem = bgfx::alloc(static_cast<uint32_t>(rw * rh));
            memcpy(rmem->data, rdata, static_cast<size_t>(rw * rh));
            // Use R8 format
            roughness = bgfx::createTexture2D((uint16_t)rw, (uint16_t)rh, false, 1, bgfx::TextureFormat::R8, 0, rmem);
            stbi_image_free(rdata);
        }
    }
    return loaded;
}

void TextureComponent::Cleanup() {
    if (bgfx::isValid(diffuse)) {
        bgfx::destroy(diffuse);
        diffuse = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(roughness)) {
        bgfx::destroy(roughness);
        roughness = BGFX_INVALID_HANDLE;
    }
    loaded = false;
}


