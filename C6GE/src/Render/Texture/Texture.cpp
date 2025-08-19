#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glad/glad.h>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../../Render/Shader/Shader.h"

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

    GLuint CreateCubemapFromSingleFile(const std::string& path) {
        int width, height, channels;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
        if (!data) {
            std::cerr << "Failed to load cubemap at path: " << path << std::endl;
            return 0;
        }

        // Assume horizontal cross layout: width = 4 * face_size, height = 3 * face_size
        if (width * 3 != height * 4) {
            std::cerr << "Cubemap image dimensions invalid for horizontal cross layout" << std::endl;
            stbi_image_free(data);
            return 0;
        }

        int face_size = width / 4;

        // Determine format
        GLenum format;
        if (channels == 1) {
            format = GL_RED;
        } else if (channels == 3) {
            format = GL_RGB;
        } else if (channels == 4) {
            format = GL_RGBA;
        } else {
            std::cerr << "Unsupported number of channels for cubemap: " << channels << std::endl;
            stbi_image_free(data);
            return 0;
        }

        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

        // Face positions for horizontal cross
        struct Face {
            int x, y;
            GLenum target;
        };

        Face faces[6] = {
            {face_size, 0, GL_TEXTURE_CUBE_MAP_POSITIVE_Y},          // +Y
            {face_size, 2 * face_size, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y}, // -Y
            {0, face_size, GL_TEXTURE_CUBE_MAP_NEGATIVE_X},          // -X
            {face_size, face_size, GL_TEXTURE_CUBE_MAP_POSITIVE_Z},  // +Z
            {2 * face_size, face_size, GL_TEXTURE_CUBE_MAP_POSITIVE_X}, // +X
            {3 * face_size, face_size, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z}  // -Z
        };

        for (int i = 0; i < 6; ++i) {
            unsigned char* face_data = new unsigned char[face_size * face_size * channels];
            bool needs_flip = (faces[i].target != GL_TEXTURE_CUBE_MAP_POSITIVE_Y && faces[i].target != GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
            for (int row = 0; row < face_size; ++row) {
                int src_row = needs_flip ? (face_size - 1 - row) : row;
                unsigned char* dest_ptr = face_data + row * face_size * channels;
                unsigned char* src_ptr = data + (faces[i].y + src_row) * width * channels + faces[i].x * channels;
                if (needs_flip) {
                    // Horizontal flip: copy in reverse
                    for (int col = 0; col < face_size; ++col) {
                        memcpy(dest_ptr + col * channels, src_ptr + (face_size - 1 - col) * channels, channels);
                    }
                } else {
                    memcpy(dest_ptr, src_ptr, face_size * channels);
                }
            }
            glTexImage2D(faces[i].target, 0, format, face_size, face_size, 0, format, GL_UNSIGNED_BYTE, face_data);
            delete[] face_data;
        }

        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);

        return textureID;
    }

    GLuint CreateTexture(unsigned char* data, int width, int height, int channels) {
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        // Set texture filtering/wrapping parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Determine format
        GLenum format;
        if (channels == 1) {
            format = GL_RED;
        } else if (channels == 3) {
            format = GL_RGB;
        } else if (channels == 4) {
            format = GL_RGBA;
        } else {
            // Handle error: unsupported channel count
            std::cerr << "Unsupported number of channels: " << channels << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        }

        // Upload texture to GPU
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data); // Cleanup after uploading

    return textureID;
}

GLuint CreateCubemapFromHDR(const std::string& path) {
    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;
    float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load HDR image at path: " << path << std::endl;
        return 0;
    }

    // Create equirectangular texture
    GLuint equiTexture;
    glGenTextures(1, &equiTexture);
    glBindTexture(GL_TEXTURE_2D, equiTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    // Create cubemap
    GLuint cubemap;
    glGenTextures(1, &cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Framebuffer
    GLuint captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    // Load and compile shaders
    const char* vertSource = LoadShader("Assets/shaders/equirect.vert");
    GLuint vertShader = CompileShader(vertSource, Vertex);
    const char* fragSource = LoadShader("Assets/shaders/equirect.frag");
    GLuint fragShader = CompileShader(fragSource, Fragment);
    GLuint equirectProgram = CreateProgram(vertShader, fragShader);
    UseProgram(equirectProgram);

    // Cube VAO
    float vertices[] = {
        // positions
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // Projection matrix
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))
    };

    // Render to each face
    glViewport(0, 0, 512, 512);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i) {
        UseProgram(equirectProgram);
        GLint projLoc = glGetUniformLocation(equirectProgram, "projection");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(captureProjection));
        GLint viewLoc = glGetUniformLocation(equirectProgram, "view");
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(captureViews[i]));
        SetShaderUniformInt(equirectProgram, "equirectangularMap", 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(VAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, equiTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteProgram(equirectProgram);

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &equiTexture);
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);

    return cubemap;
}
}