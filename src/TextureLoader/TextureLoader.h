#pragma once

#include <bgfx/bgfx.h>
#include <string>
#include <iostream>
#include <vector>

namespace C6GE {
    class TextureLoader {
    public:
        TextureLoader();
        ~TextureLoader();
        
        // Load texture from file path
        bgfx::TextureHandle loadTexture(const std::string& filePath);
        
        // Reload texture (destroy old and load new)
        bgfx::TextureHandle reloadTexture(const std::string& filePath, bgfx::TextureHandle oldTexture);
        
        // Check if texture loading was successful
        bool isTextureValid(bgfx::TextureHandle texture) const;
        
        // Get texture info
        void getTextureInfo(bgfx::TextureHandle texture, bgfx::TextureInfo& info) const;
        
        // Cleanup texture
        void destroyTexture(bgfx::TextureHandle texture);
        
        // Get available texture paths
        std::vector<std::string> getAvailableTextures() const;
        
        // Get current texture name from handle
        std::string getTextureName(bgfx::TextureHandle texture) const;
        
    private:
        // Helper function to load texture data
        bgfx::TextureHandle loadTextureInternal(const std::string& filePath);
        
        // Vector to track loaded textures and their file paths
        struct TextureInfo {
            bgfx::TextureHandle handle;
            std::string filePath;
            
            TextureInfo(bgfx::TextureHandle h, const std::string& path) 
                : handle(h), filePath(path) {}
        };
        std::vector<TextureInfo> m_loadedTextures;
    };
}
