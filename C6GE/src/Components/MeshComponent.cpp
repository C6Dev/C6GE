// GLAD include removed - using bgfx for OpenGL context management
#include <cmath>
#include "../Components/MeshComponent.h"
#include <iostream>

// Define M_PI for Windows if not already defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Forward declarations of your texture loading utilities
GLuint LoadTextureFromFile(const std::string& filename, const std::string& directory);

// Stub implementations for OpenGL functions (these will be replaced by bgfx later)
extern "C" {
    GLboolean gladLoadGL() { return GL_TRUE; }
    
    void glEnable(GLenum cap) {}
    void glDisable(GLenum cap) {}
    void glDepthMask(GLboolean flag) {}
    void glLineWidth(GLfloat width) {}
    GLuint glCreateProgram() { return 1; }
    GLuint glCreateShader(GLenum type) { return 1; }
    void glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) {}
    void glCompileShader(GLuint shader) {}
    void glGetShaderiv(GLuint shader, GLenum pname, GLint* params) { *params = GL_TRUE; }
    void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {}
    void glAttachShader(GLuint program, GLuint shader) {}
    void glLinkProgram(GLuint program) {}
    void glGetProgramiv(GLuint program, GLenum pname, GLint* params) { *params = GL_TRUE; }
    void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {}
    void glDeleteShader(GLuint shader) {}
    void glDeleteProgram(GLuint program) {}
    void glUseProgram(GLuint program) {}
    GLint glGetUniformLocation(GLuint program, const GLchar* name) { return 0; }
    void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {}
    void glUniform3fv(GLint location, GLsizei count, const GLfloat* value) {}
    void glUniform1f(GLint location, GLfloat v0) {}
    void glUniform1i(GLint location, GLint v0) {}
    void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {}
    void glGenVertexArrays(GLsizei n, GLuint* arrays) { for(GLsizei i = 0; i < n; i++) arrays[i] = i + 1; }
    void glBindVertexArray(GLuint array) {}
    void glGenBuffers(GLsizei n, GLuint* buffers) { for(GLsizei i = 0; i < n; i++) buffers[i] = i + 1; }
    void glBindBuffer(GLenum target, GLuint buffer) {}
    void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {}
    void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) {}
    void glEnableVertexAttribArray(GLuint index) {}
    void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {}
    void glDrawArrays(GLenum mode, GLint first, GLsizei count) {}
    void glGenTextures(GLsizei n, GLuint* textures) { for(GLsizei i = 0; i < n; i++) textures[i] = i + 1; }
    void glBindTexture(GLenum target, GLuint texture) {}
    void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) {}
    void glTexParameteri(GLenum target, GLenum pname, GLint param) {}
    void glGenerateMipmap(GLenum target) {}
    void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {}
    void glGetIntegerv(GLenum pname, GLint* params) { *params = 0; }
    void glGenFramebuffers(GLsizei n, GLuint* framebuffers) { for(GLsizei i = 0; i < n; i++) framebuffers[i] = i + 1; }
    void glBindFramebuffer(GLenum target, GLuint framebuffer) {}
    void glTexImage2DMultisample(GLenum target, GLsizei samples, GLint internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {}
    void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {}
    void glGenRenderbuffers(GLsizei n, GLuint* renderbuffers) { for(GLsizei i = 0; i < n; i++) renderbuffers[i] = i + 1; }
    void glBindRenderbuffer(GLenum target, GLuint renderbuffer) {}
    void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {}
    void glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {}
    void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {}
    GLenum glCheckFramebufferStatus(GLenum target) { return GL_FRAMEBUFFER_COMPLETE; }
    void glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {}
    void glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {}
    void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {}
    void glVertexAttribDivisor(GLuint index, GLuint divisor) {}
    void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei primcount) {}
    void glDeleteVertexArrays(GLsizei n, const GLuint* arrays) {}
    void glDeleteBuffers(GLsizei n, const GLuint* buffers) {}
    void glDeleteTextures(GLsizei n, const GLuint* textures) {}
    void glClear(GLbitfield mask) {}
    void glActiveTexture(GLenum texture) {}
}

MeshComponent CreateMesh(const GLfloat* vertices, size_t vertexSize, const GLuint* indices, size_t indexCount, bool WithColor, bool WithTexture) {
    GLuint vao, vbo, ebo;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertexSize, vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(GLuint), indices, GL_STATIC_DRAW);

    int floatCount = 3 + 3; // Position + Normal
    if (WithColor) floatCount += 3;
    if (WithTexture) floatCount += 2;
    GLsizei stride = floatCount * sizeof(float);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    size_t offset = 6; // After pos + normal

    // Color
    if (WithColor) {
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(offset * sizeof(float)));
        glEnableVertexAttribArray(2);
        offset += 3;
    }

    // Texture
    if (WithTexture) {
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)(offset * sizeof(float)));
        glEnableVertexAttribArray(3);
    }

    glBindVertexArray(0);

    return MeshComponent(vao, vbo, ebo, indexCount);
}

MeshComponent CreateCubeMesh() {
    static const GLfloat vertices[] = {
        // positions          // normals           // colors           // texture coords
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f,
        
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f,
        
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f,
        
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f,
        
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f,
        
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f
    };

    static const GLuint indices[] = {
        0,  1,  2,    2,  3,  0,   // front
        4,  5,  6,    6,  7,  4,   // back
        8,  9,  10,   10, 11, 8,   // left
        12, 13, 14,   14, 15, 12,  // right
        16, 17, 18,   18, 19, 16,  // bottom
        20, 21, 22,   22, 23, 20   // top
    };

    return CreateMesh(vertices, sizeof(vertices), indices, sizeof(indices) / sizeof(GLuint), true, true);
}

MeshComponent CreatePlaneMesh() {
    static const GLfloat vertices[] = {
        // positions          // normals           // colors           // texture coords
        -1.0f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
         1.0f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
         1.0f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
        -1.0f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f
    };

    static const GLuint indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    return CreateMesh(vertices, sizeof(vertices), indices, sizeof(indices) / sizeof(GLuint), true, true);
}

MeshComponent CreateSphereMesh(int segments) {
    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;

    // Generate sphere vertices
    for (int i = 0; i <= segments; ++i) {
        float lat = M_PI * (-0.5f + (float)i / segments);
        float y = sin(lat);
        float r = cos(lat);

        for (int j = 0; j <= segments; ++j) {
            float lon = 2 * M_PI * (float)j / segments;
            float x = cos(lon) * r;
            float z = sin(lon) * r;

            // Position
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // Normal (same as position for unit sphere)
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // Color
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);

            // Texture coordinates
            vertices.push_back((float)j / segments);
            vertices.push_back((float)i / segments);
        }
    }

    // Generate indices
    for (int i = 0; i < segments; ++i) {
        for (int j = 0; j < segments; ++j) {
            int first = i * (segments + 1) + j;
            int second = first + segments + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    return CreateMesh(vertices.data(), vertices.size() * sizeof(GLfloat), indices.data(), indices.size(), true, true);
}

// Static factory methods
MeshComponent MeshComponent::CreateCube() {
    return CreateCubeMesh();
}

MeshComponent MeshComponent::CreatePlane() {
    return CreatePlaneMesh();
}

MeshComponent MeshComponent::CreateSphere(int segments) {
    return CreateSphereMesh(segments);
}

MeshComponent MeshComponent::CreateMesh(const GLfloat* vertices, size_t vertexSize, const GLuint* indices, size_t indexCount, bool WithColor, bool WithTexture) {
    return ::CreateMesh(vertices, vertexSize, indices, indexCount, WithColor, WithTexture);
}

// Utility functions
MeshComponent CreateQuad() {
    return CreatePlaneMesh();
}

MeshComponent CreateTemple() {
    return CreateCubeMesh(); // Temporary replacement
}

GLuint LoadTextureFromFile(const std::string& filename, const std::string& directory) {
    // Placeholder implementation
    return 0;
}