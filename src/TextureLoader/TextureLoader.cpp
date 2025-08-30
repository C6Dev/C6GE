#include "TextureLoader.h"
#include <bx/file.h>
#include <bx/macros.h>
#include <bimg/bimg.h>
#include <bimg/decode.h>
#include <entry/entry.h>

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
        // Cleanup if needed
    }
    
    bgfx::TextureHandle TextureLoader::loadTexture(const std::string& filePath) {
        return loadTextureInternal(filePath);
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
        }
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
