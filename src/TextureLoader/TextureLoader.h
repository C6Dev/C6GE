#pragma once

#include <bgfx/bgfx.h>
#include <string>
#include <iostream>

namespace C6GE {
    class TextureLoader {
    public:
        TextureLoader();
        ~TextureLoader();
        
        // Load texture from file path
        bgfx::TextureHandle loadTexture(const std::string& filePath);
        
        // Check if texture loading was successful
        bool isTextureValid(bgfx::TextureHandle texture) const;
        
        // Get texture info
        void getTextureInfo(bgfx::TextureHandle texture, bgfx::TextureInfo& info) const;
        
        // Cleanup texture
        void destroyTexture(bgfx::TextureHandle texture);
        
    private:
        // Helper function to load texture data
        bgfx::TextureHandle loadTextureInternal(const std::string& filePath);
    };
}
