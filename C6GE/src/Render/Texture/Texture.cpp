#include "Texture.h"
#include "../../../Libraries/Include/stb/stb_image.h"

#include <glad/glad.h>

namespace C6GE {

    unsigned char* LoadTexture(const std::string& path, int& widthImg, int& heightImg, int& numColCh) {
        unsigned char* data = stbi_load(path.c_str(), &widthImg, &heightImg, &numColCh, 0);
        if (!data) {
            // Handle the error if the image couldn't be loaded
            std::cerr << "Failed to load texture at path: " << path << std::endl;
            return nullptr;
        }
        return data;
    }

    GLuint CreateTexture(unsigned char* data, int width, int height, int channels) {
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        // Set texture filtering/wrapping parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);	
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Determine format
        GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

        // Upload texture to GPU
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(data); // Cleanup after uploading

        return textureID;
    }
}