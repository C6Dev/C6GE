#include "TextureLoader.h"
#include <bx/file.h>
#include <bx/macros.h>
#include <bimg/bimg.h>
#include <bimg/decode.h>
#include <entry/entry.h>
#include <filesystem>

namespace C6GE {
    // Callback for releasing image memory
    static void imageReleaseCb(void* _ptr, void* _userData)
    {
        BX_UNUSED(_ptr);
        bimg::ImageContainer* imageContainer = (bimg::ImageContainer*)_userData;
        bimg::imageFree(imageContainer);
    }
    
    TextureLoader::TextureLoader() {
        // Initialize texture loader
    }
    
    TextureLoader::~TextureLoader() {
        // Don't destroy textures here as they're already destroyed in main cleanup
        // Just clear the tracking vector
        m_loadedTextures.clear();
    }
    
    bgfx::TextureHandle TextureLoader::loadTexture(const std::string& filePath) {
        bgfx::TextureHandle texture = loadTextureInternal(filePath);
        if (bgfx::isValid(texture)) {
            m_loadedTextures.emplace_back(texture, filePath);
        }
        return texture;
    }
    
    bgfx::TextureHandle TextureLoader::reloadTexture(const std::string& filePath, bgfx::TextureHandle oldTexture) {
        // Destroy old texture if valid
        if (bgfx::isValid(oldTexture)) {
            bgfx::destroy(oldTexture);
            // Remove from tracking
            m_loadedTextures.erase(
                std::remove_if(m_loadedTextures.begin(), m_loadedTextures.end(),
                    [oldTexture](const TextureInfo& info) { return info.handle.idx == oldTexture.idx; }),
                m_loadedTextures.end()
            );
        }
        
        // Load new texture
        bgfx::TextureHandle newTexture = loadTextureInternal(filePath);
        if (bgfx::isValid(newTexture)) {
            m_loadedTextures.emplace_back(newTexture, filePath);
        }
        return newTexture;
    }
    
    bool TextureLoader::isTextureValid(bgfx::TextureHandle texture) const {
        return bgfx::isValid(texture);
    }
    
    void TextureLoader::getTextureInfo(bgfx::TextureHandle texture, bgfx::TextureInfo& info) const {
        if (bgfx::isValid(texture)) {
            bgfx::calcTextureSize(info, 0, 0, 0, false, false, 1, bgfx::TextureFormat::RGBA8);
        }
    }
    
    void TextureLoader::destroyTexture(bgfx::TextureHandle texture) {
        if (bgfx::isValid(texture)) {
            bgfx::destroy(texture);
            // Remove from tracking
            m_loadedTextures.erase(
                std::remove_if(m_loadedTextures.begin(), m_loadedTextures.end(),
                    [texture](const TextureInfo& info) { return info.handle.idx == texture.idx; }),
                m_loadedTextures.end()
            );
        }
    }
    
    std::vector<std::string> TextureLoader::getAvailableTextures() const {
        std::vector<std::string> textures;
        std::string textureDir = "assets/textures/";
        
        try {
            for (const auto& entry : std::filesystem::directory_iterator(textureDir)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".exr") {
                        textures.push_back(entry.path().string());
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cout << "Error reading texture directory: " << e.what() << std::endl;
        }
        
        return textures;
    }
    
    std::string TextureLoader::getTextureName(bgfx::TextureHandle texture) const {
        for (const auto& textureInfo : m_loadedTextures) {
            if (textureInfo.handle.idx == texture.idx) {
                return std::filesystem::path(textureInfo.filePath).filename().string();
            }
        }
        return "Unknown";
    }
    
    bgfx::TextureHandle TextureLoader::loadTextureInternal(const std::string& filePath) {
        // Create a file reader
        bx::FileReader reader;
        bx::Error err;
        if (!reader.open(filePath.c_str(), &err)) {
            std::cout << "Failed to open texture file: " << filePath << std::endl;
            return BGFX_INVALID_HANDLE;
        }
        
        // Get file size
        uint32_t size = (uint32_t)reader.seek(0, bx::Whence::End);
        reader.seek(0, bx::Whence::Begin);
        
        // Allocate memory and read file
        void* data = malloc(size);
        if (data == nullptr) {
            std::cout << "Failed to allocate memory for texture: " << filePath << std::endl;
            reader.close();
            return BGFX_INVALID_HANDLE;
        }
        
        reader.read(data, size, &err);
        reader.close();
        
        // Parse image
        bimg::ImageContainer* imageContainer = bimg::imageParse(entry::getAllocator(), data, size);
        if (imageContainer == nullptr) {
            std::cout << "Failed to parse texture: " << filePath << std::endl;
            free(data);
            return BGFX_INVALID_HANDLE;
        }
        
        // Create texture
        // Check if texture format is valid
        uint64_t flags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE;
        if (!bgfx::isTextureValid(0, false, imageContainer->m_numLayers, bgfx::TextureFormat::Enum(imageContainer->m_format), flags)) {
            std::cout << "Texture format not supported: " << imageContainer->m_format << std::endl;
            bimg::imageFree(imageContainer);
            free(data);
            return BGFX_INVALID_HANDLE;
        }
        
        // Create memory reference with proper cleanup callback
        const bgfx::Memory* mem = bgfx::makeRef(
            imageContainer->m_data,
            imageContainer->m_size,
            imageReleaseCb,
            imageContainer
        );
        
        bgfx::TextureHandle texture = bgfx::createTexture2D(
            uint16_t(imageContainer->m_width),
            uint16_t(imageContainer->m_height),
            1 < imageContainer->m_numMips,
            imageContainer->m_numLayers,
            bgfx::TextureFormat::Enum(imageContainer->m_format),
            flags,
            mem
        );
        
        // Cleanup - unload the original data since bgfx::makeRef takes ownership
        free(data);
        
        if (!bgfx::isValid(texture)) {
            std::cout << "Failed to create texture: " << filePath << std::endl;
            return BGFX_INVALID_HANDLE;
        }
        
        return texture;
    }
}
