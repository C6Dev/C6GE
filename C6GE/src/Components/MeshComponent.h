#pragma once
#include <cstddef>  // for size_t and ptrdiff_t
// GLAD include removed - using bgfx for OpenGL context management
// OpenGL type definitions
using GLfloat = float;
using GLuint = unsigned int;
using GLint = int;
using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbyte = signed char;
using GLubyte = unsigned char;
using GLshort = signed short;
using GLushort = unsigned short;
using GLsizei = int;
using GLclampf = float;
using GLclampd = double;
using GLvoid = void;
using GLchar = char;
using GLsizeiptr = ptrdiff_t;
using GLbitfield = unsigned int;

// OpenGL constants
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_DEPTH_TEST 0x0B71
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_MAX_SAMPLES 0x8D57
#define GL_FRAMEBUFFER 0x8D40
#define GL_TEXTURE_2D_MULTISAMPLE 0x9100
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_RENDERBUFFER 0x8D41
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_RGBA 0x1908
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_LINEAR 0x2601
#define GL_TRIANGLES 0x0004
#define GL_UNSIGNED_INT 0x1405
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_GEOMETRY_SHADER 0x8DD9
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_ARRAY_BUFFER 0x8892
#define GL_FLOAT 0x1406
#define GL_RED 0x1903
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 0x851A
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_REPEAT 0x2901
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_RGB32F 0x8815
#define GL_RGB16F 0x881B
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_TEXTURE0 0x84C0

// OpenGL function declarations (these will be replaced by bgfx later)
extern "C" {
    void glEnable(GLenum cap);
    void glDisable(GLenum cap);
    void glDepthMask(GLboolean flag);
    void glLineWidth(GLfloat width);
    GLuint glCreateProgram();
    GLuint glCreateShader(GLenum type);
    void glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
    void glCompileShader(GLuint shader);
    void glGetShaderiv(GLuint shader, GLenum pname, GLint* params);
    void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
    void glAttachShader(GLuint program, GLuint shader);
    void glLinkProgram(GLuint program);
    void glGetProgramiv(GLuint program, GLenum pname, GLint* params);
    void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
    void glDeleteShader(GLuint shader);
    void glDeleteProgram(GLuint program);
    void glUseProgram(GLuint program);
    GLint glGetUniformLocation(GLuint program, const GLchar* name);
    void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    void glUniform3fv(GLint location, GLsizei count, const GLfloat* value);
    void glUniform1f(GLint location, GLfloat v0);
    void glUniform1i(GLint location, GLint v0);
    void glGenVertexArrays(GLsizei n, GLuint* arrays);
    void glBindVertexArray(GLuint array);
    void glGenBuffers(GLsizei n, GLuint* buffers);
    void glBindBuffer(GLenum target, GLuint buffer);
    void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
    void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
    void glEnableVertexAttribArray(GLuint index);
    void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
    void glGenTextures(GLsizei n, GLuint* textures);
    void glBindTexture(GLenum target, GLuint texture);
    void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
    void glTexParameteri(GLenum target, GLenum pname, GLint param);
    void glGenerateMipmap(GLenum target);
    void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
    void glGetIntegerv(GLenum pname, GLint* params);
    GLboolean gladLoadGL();
    void glGenFramebuffers(GLsizei n, GLuint* framebuffers);
    void glBindFramebuffer(GLenum target, GLuint framebuffer);
    void glTexImage2DMultisample(GLenum target, GLsizei samples, GLint internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
    void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
    void glGenRenderbuffers(GLsizei n, GLuint* renderbuffers);
    void glBindRenderbuffer(GLenum target, GLuint renderbuffer);
    void glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
    void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
    GLenum glCheckFramebufferStatus(GLenum target);
    void glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers);
    void glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
    void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
    void glVertexAttribDivisor(GLuint index, GLuint divisor);
    void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei primcount);
    void glDeleteVertexArrays(GLsizei n, const GLuint* arrays);
    void glDeleteBuffers(GLsizei n, const GLuint* buffers);
    void glDeleteTextures(GLsizei n, const GLuint* textures);
    void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
    void glClear(GLbitfield mask);
    void glActiveTexture(GLenum texture);
    void glDrawArrays(GLenum mode, GLint first, GLsizei count);
    void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
}

// cstddef already included above
#include <vector>
#include <string>
#include <glm/glm.hpp>

struct MeshComponent {
    GLuint VAO, VBO, EBO;
    size_t IndexCount;
    size_t vertexCount; // Add this field for compatibility
    std::string Name;
    bool HasTexture;
    bool HasColor;

    MeshComponent(GLuint vao, GLuint vbo, GLuint ebo, size_t count)
        : VAO(vao), VBO(vbo), EBO(ebo), IndexCount(count), vertexCount(count), HasTexture(true), HasColor(true) {}

    // Factory functions for creating common meshes
    static MeshComponent CreateCube();
    static MeshComponent CreatePlane();
    static MeshComponent CreateSphere(int segments = 32);
    static MeshComponent CreateMesh(const GLfloat* vertices, size_t vertexSize, const GLuint* indices, size_t indexCount, bool WithColor = true, bool WithTexture = true);
};

// Utility functions
MeshComponent CreateCubeMesh();
MeshComponent CreatePlaneMesh();
MeshComponent CreateSphereMesh(int segments = 32);
MeshComponent CreateQuad(); // Add this function
MeshComponent CreateTemple(); // Add this function
GLuint LoadTextureFromFile(const std::string& filename, const std::string& directory);
