#include "Logging/Log.h"
#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
// OpenGL headers for OpenGL renderer
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>

// OpenGL constants that might not be defined in basic headers
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_TEXTURE2
#define GL_TEXTURE2 0x84C2
#endif
#ifndef GL_TEXTURE3
#define GL_TEXTURE3 0x84C3
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif

#include "Render.h"
#include "../Window/Window.h"
#include "../ECS/Object/Object.h"
#include "../Components/CameraComponent.h"
#include "../Components/LightComponent.h"
#include "../Components/SpecularTextureComponent.h"
#include "../Components/CubemapComponent.h"
#include "../Components/ScaleComponent.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <stb_image.h>
#include "../Components/InstanceComponent.h"
#include "../Components/CubeComponent.h"
#include "../Components/MetaballComponent.h"
#include "VertexLayouts.h"
#include "../Logging/Log.h"
#include "../Window/Window.h"
#include "../Components/TextureComponent.h"

// BGFX vertex structure
struct BGFXVertex {
    float x, y, z;    // position
    float nx, ny, nz; // normal
    float r, g, b;    // color
    float u, v;       // texture coordinates
};

// BGFX vertex layout
bgfx::VertexLayout bgfxVertexLayout;

// Simple triangle vertices for BGFX (position only) - larger triangle
static float s_triangleVertices[] = {
     0.0f,  0.8f, 0.0f,  // top
    -0.8f, -0.8f, 0.0f,  // bottom left
     0.8f, -0.8f, 0.0f   // bottom right
};

static const uint16_t s_triangleIndices[] = {
    0, 1, 2  // single triangle
};

// BGFX resources
bgfx::VertexBufferHandle bgfxVertexBuffer = BGFX_INVALID_HANDLE;
bgfx::IndexBufferHandle bgfxIndexBuffer = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle bgfxSimpleProgram = BGFX_INVALID_HANDLE;

// HDR resources
static bgfx::ProgramHandle bgfxTonemapProgram = BGFX_INVALID_HANDLE;
static bgfx::FrameBufferHandle g_hdrFb = BGFX_INVALID_HANDLE;
static bgfx::TextureHandle g_hdrColor = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle u_exposure = BGFX_INVALID_HANDLE;
static uint16_t g_backWidth = 0, g_backHeight = 0;

// Cube geometry data (from BGFX 01-cubes example)
struct PosColorVertex {
    float x, y, z;
    uint32_t abgr;
    
    static void init() {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
            .end();
    }
    
    static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosColorVertex::ms_layout;

static PosColorVertex s_cubeVertices[] = {
    {-1.0f,  1.0f,  1.0f, 0xff000000 },
    { 1.0f,  1.0f,  1.0f, 0xff0000ff },
    {-1.0f, -1.0f,  1.0f, 0xff00ff00 },
    { 1.0f, -1.0f,  1.0f, 0xff00ffff },
    {-1.0f,  1.0f, -1.0f, 0xffff0000 },
    { 1.0f,  1.0f, -1.0f, 0xffff00ff },
    {-1.0f, -1.0f, -1.0f, 0xffffff00 },
    { 1.0f, -1.0f, -1.0f, 0xffffffff },
};

static const uint16_t s_cubeTriList[] = {
    0, 1, 2, // 0
    1, 3, 2,
    4, 6, 5, // 2
    5, 6, 7,
    0, 2, 4, // 4
    4, 2, 6,
    1, 5, 3, // 6
    5, 7, 3,
    0, 4, 1, // 8
    4, 5, 1,
    2, 3, 6, // 10
    6, 3, 7,
};

// Cube rendering resources
bgfx::VertexBufferHandle bgfxCubeVertexBuffer = BGFX_INVALID_HANDLE;
bgfx::IndexBufferHandle bgfxCubeIndexBuffer = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle bgfxCubeProgram = BGFX_INVALID_HANDLE;

// Metaball rendering resources
bgfx::ProgramHandle bgfxMetaballProgram = BGFX_INVALID_HANDLE;

// Metaball structures
struct PosNormalColorVertex
{
    float m_pos[3];
    float m_normal[3];
    uint32_t m_abgr;

    static void init()
    {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal,   3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
            .end();
    };

    static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosNormalColorVertex::ms_layout;

// Raymarching vertex structure
struct PosColorTexCoord0Vertex
{
    float m_x;
    float m_y;
    float m_z;
    uint32_t m_abgr;
    float m_u;
    float m_v;

    static void init()
    {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }

    static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosColorTexCoord0Vertex::ms_layout;



struct Grid
{
    Grid()
        : m_val(0.0f)
    {
        m_normal[0] = 0.0f;
        m_normal[1] = 0.0f;
        m_normal[2] = 0.0f;
    }

    float m_val;
    float m_normal[3];
};

// Metaball constants
constexpr uint32_t kMaxDims  = 32;
constexpr float    kMaxDimsF = float(kMaxDims);

// Metaball data
Grid* g_metaballGrid = nullptr;
int64_t g_metaballTimeOffset = 0;

// Raymarching resources
bgfx::ProgramHandle bgfxRaymarchProgram = BGFX_INVALID_HANDLE;
bgfx::UniformHandle bgfxRaymarchMtx = BGFX_INVALID_HANDLE;
bgfx::UniformHandle bgfxRaymarchLightDirTime = BGFX_INVALID_HANDLE;
int64_t g_raymarchTimeOffset = 0;

// Mesh rendering resources
bgfx::ProgramHandle bgfxMeshProgram = BGFX_INVALID_HANDLE;
bgfx::UniformHandle bgfxMeshMtx = BGFX_INVALID_HANDLE;
bgfx::UniformHandle bgfxMeshLightDirTime = BGFX_INVALID_HANDLE;
int64_t g_meshTimeOffset = 0;



// Cube objects for rendering
std::vector<C6GE::CubeComponent> g_cubes;

// Forward declarations
namespace C6GE {
    void InitBGFXResources();
    void RenderBGFXCube();
    void InitCubeResources();
    void RenderCubes();
    void CreateCube(const glm::vec3& position, const glm::vec4& color);
    void InitMetaballResources();
    void RenderMetaballs();
    void InitRaymarchResources();
    void RenderRaymarch();
    void InitMeshResources();
    void RenderMesh();
    void renderScreenSpaceQuad(uint8_t _view, bgfx::ProgramHandle _program, float _x, float _y, float _width, float _height);
    bgfx::ShaderHandle CreateShaderFromBinary(const uint8_t* data, uint32_t size, const char* name);
}

// Metaball lookup tables (from BGFX 02-metaballs example)
static const uint16_t s_edges[256] =
{
	0x000, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
	0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
	0x190, 0x099, 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
	0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
	0x230, 0x339, 0x033, 0x13a, 0x636, 0x73f, 0x435, 0x53c,
	0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
	0x3a0, 0x2a9, 0x1a3, 0x0aa, 0x7a6, 0x6af, 0x5a5, 0x4ac,
	0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
	0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
	0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
	0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0x0ff, 0x3f5, 0x2fc,
	0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
	0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x055, 0x15c,
	0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
	0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0x0cc,
	0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
	0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
	0x0cc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
	0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
	0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
	0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
	0x2fc, 0x3f5, 0x0ff, 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
	0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
	0x36c, 0x265, 0x16f, 0x066, 0x76a, 0x663, 0x569, 0x460,
	0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
	0x4ac, 0x5a5, 0x6af, 0x7a6, 0x0aa, 0x1a3, 0x2a9, 0x3a0,
	0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
	0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x033, 0x339, 0x230,
	0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
	0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x099, 0x190,
	0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
	0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x000,
};

static const float s_cube[8][3] =
{
	{ 0.0f, 1.0f, 1.0f }, // 0
	{ 1.0f, 1.0f, 1.0f }, // 1
	{ 1.0f, 1.0f, 0.0f }, // 2
	{ 0.0f, 1.0f, 0.0f }, // 3
	{ 0.0f, 0.0f, 1.0f }, // 4
	{ 1.0f, 0.0f, 1.0f }, // 5
	{ 1.0f, 0.0f, 0.0f }, // 6
	{ 0.0f, 0.0f, 0.0f }, // 7
};

// Metaball triangulation indices (simplified - just the first few cases)
static const int8_t s_indices[256][16] = {
	{  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{   0,  8,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{   0,  1,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{   1,  8,  3,  9,  8,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{   1,  2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{   0,  8,  3,  1,  2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{   9,  2, 10,  0,  2,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{   2,  8,  3,  2, 10,  8, 10,  9,  8, -1, -1, -1, -1, -1, -1, -1 },
	{   3, 11,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{   0, 11,  2,  8, 11,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{   1,  9,  0,  2,  3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{   1, 11,  2,  1,  9, 11,  9,  8, 11, -1, -1, -1, -1, -1, -1, -1 },
	{   3, 10,  1, 11, 10,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{   0, 10,  1,  0,  8, 10,  8, 11, 10, -1, -1, -1, -1, -1, -1, -1 },
	{   3,  9,  0,  3, 11,  9, 11, 10,  9, -1, -1, -1, -1, -1, -1, -1 },
	{   9,  8, 10, 10,  8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	// ... rest of the array would be filled with the complete triangulation data
	// For now, just use the first 16 cases and fill the rest with -1
};

// Pre-compiled BGFX shaders for solid color rendering
// These are minimal shaders compiled for DirectX11/OpenGL

// Simple vertex shader (pre-compiled for DX11)
static const uint8_t s_vertexShaderDX11[] = {
    0x56, 0x53, 0x48, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x01, 0x83, 0xf2, 0xe1, 0x01, 0x00, 0x0f, 0x75,
    0x5f, 0x6d, 0x6f, 0x64, 0x65, 0x6c, 0x56, 0x69, 0x65, 0x77, 0x50, 0x72, 0x6f, 0x6a, 0x09, 0x01,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x02, 0x00, 0x00, 0x44, 0x58, 0x42, 0x43,
    0x26, 0xb0, 0x69, 0x8d, 0x39, 0x3c, 0x9a, 0x36, 0x81, 0x14, 0x89, 0x85, 0x3c, 0x1d, 0x68, 0x54,
    0x01, 0x00, 0x00, 0x00, 0xf8, 0x02, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00,
    0x60, 0x00, 0x00, 0x00, 0x94, 0x00, 0x00, 0x00, 0x49, 0x53, 0x47, 0x4e, 0x2c, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x07, 0x00, 0x00,
    0x50, 0x4f, 0x53, 0x49, 0x54, 0x49, 0x4f, 0x4e, 0x00, 0xab, 0xab, 0xab, 0x4f, 0x53, 0x47, 0x4e,
    0x2c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0f, 0x00, 0x00, 0x00, 0x53, 0x56, 0x5f, 0x50, 0x4f, 0x53, 0x49, 0x54, 0x49, 0x4f, 0x4e, 0x00,
    0x53, 0x48, 0x44, 0x52, 0x5c, 0x02, 0x00, 0x00, 0x40, 0x00, 0x01, 0x00, 0x97, 0x00, 0x00, 0x00,
    0x59, 0x00, 0x00, 0x04, 0x46, 0x8e, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x5f, 0x00, 0x00, 0x03, 0x72, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x67, 0x00, 0x00, 0x04,
    0xf2, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x02,
    0x01, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x08, 0x72, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x56, 0x15, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x82, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x0a, 0x72, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x46, 0x82, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x10, 0x10, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x46, 0x02, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x0a,
    0x72, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x82, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0xa6, 0x1a, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x02, 0x10, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x72, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x46, 0x02, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x82, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x08, 0x82, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3a, 0x82, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00
};

// Simple fragment shader (pre-compiled for DX11)
static const uint8_t s_fragmentShaderDX11[] = {
    0x46, 0x53, 0x48, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xbc, 0x01, 0x00, 0x00, 0x44, 0x58, 0x42, 0x43, 0x5d, 0x4a, 0x52, 0x44, 0x42, 0x58,
    0x2e, 0x74, 0xaa, 0xb4, 0xd1, 0x67, 0xb8, 0x1e, 0x78, 0xc8, 0x01, 0x00, 0x00, 0x00, 0xbc, 0x01,
    0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xb4, 0x00,
    0x00, 0x00, 0x49, 0x53, 0x47, 0x4e, 0x4c, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x07,
    0x00, 0x00, 0x53, 0x56, 0x5f, 0x50, 0x4f, 0x53, 0x49, 0x54, 0x49, 0x4f, 0x4e, 0x00, 0x43, 0x4f,
    0x4c, 0x4f, 0x52, 0x00, 0xab, 0xab, 0x4f, 0x53, 0x47, 0x4e, 0x2c, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x53, 0x56,
    0x5f, 0x54, 0x41, 0x52, 0x47, 0x45, 0x54, 0x00, 0xab, 0xab, 0x53, 0x48, 0x44, 0x52, 0x00, 0x01,
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x62, 0x10, 0x00, 0x03, 0x72, 0x10,
    0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x65, 0x00, 0x00, 0x03, 0xf2, 0x20, 0x10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x36, 0x00, 0x00, 0x05, 0x72, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x12,
    0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x05, 0x82, 0x20, 0x10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x3e, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00
};

// BGFX shader handles
bgfx::ShaderHandle s_vertexShader = BGFX_INVALID_HANDLE;
bgfx::ShaderHandle s_fragmentShader = BGFX_INVALID_HANDLE;

#ifdef __APPLE__
extern "C" void* setupMetalLayer(void* nwh);
#endif

GLuint multisampleFBO, resolveFBO, colorTextureMS, colorTextureResolve, depthStencilRBOMS, quadVAO = 0, quadVBO = 0;
// postShader removed - no longer needed for BGFX-only rendering

float quadVertices[] = {
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
};

namespace C6GE {

unsigned int samples = 1; // default (no MSAA)
RendererType currentRenderer = RendererType::OpenGL; // Track current renderer

bool InitRender(unsigned int width, unsigned int height, RendererType render) {
    currentRenderer = render; // Store the renderer type
    
    if (render == RendererType::BGFX) {
		return InitBGFX();
    } 
    else { // Default: OpenGL
        // OpenGL context should already be available from GLFW
        // No need for GLAD initialization

        GLint maxSamples = 0;
        glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        samples = static_cast<unsigned int>(maxSamples);
        Log(LogLevel::info, "GPU supports up to " + std::to_string(samples) + "x MSAA");

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Create multisample FBO
        glGenFramebuffers(1, &multisampleFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, multisampleFBO);

        glGenTextures(1, &colorTextureMS);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorTextureMS);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA, width, height, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, colorTextureMS, 0);

        glGenRenderbuffers(1, &depthStencilRBOMS);
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRBOMS);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRBOMS);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[ERROR] Multisample FBO incomplete: " << status << std::endl;
            glDeleteTextures(1, &colorTextureMS);
            glDeleteRenderbuffers(1, &depthStencilRBOMS);
            glDeleteFramebuffers(1, &multisampleFBO);
            return false;
        }

        glGenFramebuffers(1, &resolveFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, resolveFBO);

        glGenTextures(1, &colorTextureResolve);
        glBindTexture(GL_TEXTURE_2D, colorTextureResolve);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTextureResolve, 0);

        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[ERROR] Resolve FBO incomplete: " << status << std::endl;
            glDeleteTextures(1, &colorTextureMS);
            glDeleteTextures(1, &colorTextureResolve);
            glDeleteRenderbuffers(1, &depthStencilRBOMS);
            glDeleteFramebuffers(1, &multisampleFBO);
            glDeleteFramebuffers(1, &resolveFBO);
            return false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Screen quad VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);

        return true;
    }
}

	// Clear the screen with a specified color
	void Clear(float r, float g, float b, float a) {
		if (currentRenderer == RendererType::BGFX) {
			ClearBGFX(r, g, b, a);
		} else {
			glClearColor(r, g, b, a);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		}
	}

	// Bind the resolve FBO
	void BindNormalFramebuffer() {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFBO);
	}

	// Unbind the resolve FBO
	void UnbindNormalFramebuffer() {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	// Bind the multisample FBO
	void BindMultisampleFramebuffer() {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, multisampleFBO);
	}

	// Unbind the multisample FBO
	void UnbindMultisampleFramebuffer() {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}

	// Present the rendered frame to the window
	void Present() {
		if (currentRenderer == RendererType::BGFX) {
			PresentBGFX();
		} else {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampleFBO);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO);
			GLint viewport[4];
			glGetIntegerv(GL_VIEWPORT, viewport);
			glBlitFramebuffer(0, 0, viewport[2], viewport[3], 0, 0, viewport[2], viewport[3], GL_COLOR_BUFFER_BIT, GL_LINEAR);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			Clear(0.0f, 0.0f, 0.0f, 1.0f);

			// Post-processing removed - no longer needed for BGFX-only rendering

			glfwSwapBuffers(glfwGetCurrentContext());
		}
	}

	void RenderObject(const std::string& name, bool useStencil, bool isOutlinePass) {
    	auto* shaderComp   = GetComponent<ShaderComponent>(name);
    	auto* meshComp     = GetComponent<MeshComponent>(name);
		auto* modelComp = GetComponent<ModelComponent>(name);
    	auto* textureComp  = GetComponent<TextureComponent>(name);
    	auto* specularComp = GetComponent<SpecularTextureComponent>(name);
		auto* cubemapComp = GetComponent<CubemapComponent>(name);
		auto* skyboxComp = GetComponent<SkyboxComponent>(name);
    	auto* transform    = GetComponent<TransformComponent>(name); // optional
    	auto* lightComp    = GetComponent<LightComponent>(name);
    	auto* instComp     = GetComponent<InstanceComponent>(name);

    	if (!shaderComp || !meshComp) return;

    	// BGFX rendering path
    	if (currentRenderer == RendererType::BGFX) {
    	    // For now, just render a simple cube for any object
    	    RenderBGFXCube();
    	    return;
    	}

    	// OpenGL rendering path continues below...

        // Configure stencil operations based on the current pass
        if (useStencil) {
            if (!isOutlinePass) {
                // First pass: Write to the stencil buffer
                glStencilFunc(GL_ALWAYS, 1, 0xFF); // Always pass stencil test and set stencil value to 1
                glStencilMask(0xFF); // Enable writing to stencil buffer
                glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE); // Replace stencil value on stencil and depth pass
            } else {
                // Second pass: Draw outline using stencil test
                glStencilFunc(GL_NOTEQUAL, 1, 0xFF); // Only pass where stencil is NOT 1 (outside the object)
                glStencilMask(0x00); // Disable writing to stencil buffer
                // Disable depth testing for outline pass to draw over everything
                glDisable(GL_DEPTH_TEST);
            }
        } else {
            // Normal rendering without stencil operations
            glStencilMask(0x00); // Disable writing to stencil buffer
            glStencilFunc(GL_ALWAYS, 1, 0xFF); // Always pass stencil test
        }

    	glUseProgram(shaderComp->ShaderProgram);

    	if (lightComp) {
        	glUniform3fv(glGetUniformLocation(shaderComp->ShaderProgram, "lightColor"), 1, glm::value_ptr(lightComp->color));
    	}

    	// Texture binding
#if 0
    	if (textureComp) {
        	glActiveTexture(GL_TEXTURE0);
        	glBindTexture(GL_TEXTURE_2D, textureComp->Texture);
        	glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "tex0"), 0);
    	}

    	if (specularComp) {
        	glActiveTexture(GL_TEXTURE1);
        	glBindTexture(GL_TEXTURE_2D, specularComp->Texture);
        	glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "specularMap"), 1);
    	}
#endif
		if (cubemapComp) {
    		glActiveTexture(GL_TEXTURE2);
    		glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapComp->Cubemap);
    		glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "cubemap"), 2);
		}
		if (skyboxComp) {
    		glActiveTexture(GL_TEXTURE3);
    		glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxComp->Cubemap);
    		glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "skybox"), 3);
		}
		if (cubemapComp || skyboxComp) {
			glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "hasCubemap"), 1);
		} else {
			glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "hasCubemap"), 0);
		}

		// Render Skybox
		if (skyboxComp) {
			glDepthFunc(GL_LEQUAL);
            glDisable(GL_CULL_FACE);
            glUseProgram(shaderComp->ShaderProgram);
            auto* cameraComp = GetComponent<CameraComponent>("camera");
            glm::mat4 view = GetViewMatrix(*cameraComp);
            glm::mat4 proj = GetProjectionMatrix();
            glUniformMatrix4fv(glGetUniformLocation(shaderComp->ShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(glm::mat4(glm::mat3(view))));
            glUniformMatrix4fv(glGetUniformLocation(shaderComp->ShaderProgram, "proj"), 1, GL_FALSE, glm::value_ptr(proj));
            auto* skyboxCubemap = GetComponent<SkyboxComponent>("skybox");
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap->Cubemap);
            glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "skybox"), 0);
            auto* skyboxMeshComp = GetComponent<MeshComponent>("skybox");
            glBindVertexArray(skyboxMeshComp->VAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(skyboxMeshComp->vertexCount), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
            glEnable(GL_CULL_FACE);
            glDepthFunc(GL_LESS);
		}

    	// Construct transform matrix
    	glm::mat4 model = glm::mat4(1.0f);
    	if (transform) {
        	model = glm::translate(model, transform->Position);
        	model = glm::rotate(model, glm::radians(transform->Rotation.x), glm::vec3(1,0,0));
        	model = glm::rotate(model, glm::radians(transform->Rotation.y), glm::vec3(0,1,0));
        	model = glm::rotate(model, glm::radians(transform->Rotation.z), glm::vec3(0,0,1));
        	model = glm::scale(model, transform->Scale);
    	} else {
        	// Default to 2D style (no depth, scale = 1)
        	model = glm::translate(model, glm::vec3(0.0f));
    	}

    	auto* scaleComp = GetComponent<ScaleComponent>(name);
    	if (scaleComp) {
        	model = glm::scale(model, scaleComp->scale);
    	}

    	bool isInstanced = instComp && !instComp->instances.empty();
    	if (!isInstanced) {
    	    GLint modelLoc = glGetUniformLocation(shaderComp->ShaderProgram, "model");
    	    if (modelLoc != -1)
    	        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    	}

		// Collect all lights
		std::vector<std::string> lightNames = GetAllObjectsWithComponent<LightComponent>();
		int numLights = std::min(static_cast<int>(lightNames.size()), 3); // MAX_LIGHTS = 3
		glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, "numLights"), numLights);

		for (int i = 0; i < numLights; ++i) {
		    auto* lComp = GetComponent<LightComponent>(lightNames[i]);
		    auto* lTrans = GetComponent<TransformComponent>(lightNames[i]);
		    if (lComp && lTrans) {
		        std::string base = "lights[" + std::to_string(i) + "]";
		        glUniform1i(glGetUniformLocation(shaderComp->ShaderProgram, (base + ".type").c_str()), lComp->type);
		        glUniform3fv(glGetUniformLocation(shaderComp->ShaderProgram, (base + ".position").c_str()), 1, glm::value_ptr(lTrans->Position));
		        glUniform3fv(glGetUniformLocation(shaderComp->ShaderProgram, (base + ".direction").c_str()), 1, glm::value_ptr(lComp->direction));
		        glUniform3fv(glGetUniformLocation(shaderComp->ShaderProgram, (base + ".color").c_str()), 1, glm::value_ptr(lComp->color));
		        glUniform1f(glGetUniformLocation(shaderComp->ShaderProgram, (base + ".intensity").c_str()), lComp->intensity);
		        glUniform1f(glGetUniformLocation(shaderComp->ShaderProgram, (base + ".cutoff").c_str()), lComp->cutoff);
		    }
		}

		auto* camera = GetComponent<CameraComponent>("camera");
		if (camera) {
			glUniform3fv(glGetUniformLocation(shaderComp->ShaderProgram, "viewPos"), 1, glm::value_ptr(camera->Transform.Position));
		}

		glm::mat4 view = (camera) ? GetViewMatrix(*camera) : glm::mat4(1.0f);
		glm::mat4 proj = C6GE::GetProjectionMatrix();

		// Set uniforms
		GLint viewLoc = glGetUniformLocation(shaderComp->ShaderProgram, "view");
		if (viewLoc != -1)
    		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

		GLint projLoc = glGetUniformLocation(shaderComp->ShaderProgram, "proj");
		if (projLoc != -1)
    		glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(proj));

    	// Draw
    	if (isInstanced) {
    	    if (instComp->instanceVBO == 0) {
    	        glGenBuffers(1, &instComp->instanceVBO);
    	    }
    	    glBindBuffer(GL_ARRAY_BUFFER, instComp->instanceVBO);
    	    glBufferData(GL_ARRAY_BUFFER, instComp->instances.size() * sizeof(glm::mat4), instComp->instances.data(), GL_DYNAMIC_DRAW);

    	    // Assume attributes 0: pos, 1: normal, 2: texcoord, so instance matrix at 3-6
    	    for (unsigned int i = 0; i < 4; ++i) {
    	        unsigned int attrib = 3 + i;
    	        glEnableVertexAttribArray(attrib);
    	        glVertexAttribPointer(attrib, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
    	        glVertexAttribDivisor(attrib, 1);
    	    }
    	    glBindBuffer(GL_ARRAY_BUFFER, 0);
    	}

		// Old OpenGL rendering code removed - now using BGFX component system
		
    	glBindVertexArray(0);

        // Restore depth test if it was disabled for outline pass
        if (useStencil && isOutlinePass) {
            glEnable(GL_DEPTH_TEST);
        }
    }

    void AddInstance(const std::string& name, const glm::mat4& transform) {
        auto* instComp = GetComponent<InstanceComponent>(name);
        if (!instComp) {
            AddComponent<InstanceComponent>(name);
            instComp = GetComponent<InstanceComponent>(name);
        }
        instComp->instances.push_back(transform);
    }

    void ClearInstances(const std::string& name) {
        auto* instComp = GetComponent<InstanceComponent>(name);
        if (instComp) {
            instComp->instances.clear();
        }
    }

bool InitBGFX() {
    Log(LogLevel::info, "Initializing BGFX...");

    // Get the window from the Window module instead of current context
    GLFWwindow* window = static_cast<GLFWwindow*>(GetWindow());
    if (!window) {
        Log(LogLevel::error, "No GLFW window available for BGFX.");
        return false;
    }

    // Get window dimensions
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    Log(LogLevel::info, "Window size: " + std::to_string(width) + "x" + std::to_string(height));

    // Make sure window is visible
    glfwShowWindow(window);
    glfwPollEvents(); // Process any pending events

    bgfx::PlatformData pd{};
    pd.ndt = nullptr;
    pd.nwh = nullptr;
    pd.context = nullptr;
    pd.backBuffer = nullptr;
    pd.backBufferDS = nullptr;
    
#ifdef _WIN32
    pd.nwh = glfwGetWin32Window(window);
#elif __APPLE__
    pd.nwh = glfwGetCocoaWindow(window); // Cocoa NSWindow*
    Log(LogLevel::info, "Got Cocoa window handle: " + std::to_string(reinterpret_cast<uintptr_t>(pd.nwh)));
    
    // Set up Metal layer to avoid deadlock issues
    void* metalLayer = setupMetalLayer(pd.nwh);
    Log(LogLevel::info, "Set up Metal layer: " + std::to_string(reinterpret_cast<uintptr_t>(metalLayer)));
#elif __linux__
    pd.nwh = (void*)(uintptr_t)glfwGetX11Window(window); // X11
#endif
    
    if (!pd.nwh) {
        Log(LogLevel::error, "Failed to get native window handle.");
        return false;
    }
    
    Log(LogLevel::info, "Setting BGFX platform data...");
    bgfx::setPlatformData(pd);

    // Try calling renderFrame before init (some platforms require this)
    bgfx::renderFrame();
    
    bgfx::Init init{};
    init.platformData = pd;  // Set platform data directly in init struct
    init.resolution.width  = static_cast<uint32_t>(width);
    init.resolution.height = static_cast<uint32_t>(height);
    init.resolution.reset  = BGFX_RESET_VSYNC;

    bool initialized = false;

#ifdef _WIN32
    // Windows: Prefer DX12 again (as requested), then DX11 -> Vulkan -> OpenGL
    const bgfx::RendererType::Enum renderers[] = {
        bgfx::RendererType::Direct3D12,
        bgfx::RendererType::Direct3D11,
        bgfx::RendererType::Vulkan,
        bgfx::RendererType::OpenGL
    };
#elif __APPLE__
    // macOS: Metal -> OpenGL
    const bgfx::RendererType::Enum renderers[] = {
        bgfx::RendererType::Metal,
        bgfx::RendererType::OpenGL
    };
#else
    // Linux: Vulkan -> OpenGL
    const bgfx::RendererType::Enum renderers[] = {
        bgfx::RendererType::Vulkan,
        bgfx::RendererType::OpenGL
    };
#endif

    for (auto r : renderers) {
        init.type = r;
        Log(LogLevel::info, "Attempting to initialize BGFX with renderer: " + std::string(bgfx::getRendererName(r)));
        
        if (bgfx::init(init)) {
            Log(LogLevel::info, "BGFX initialized successfully with renderer: " + std::string(bgfx::getRendererName(r)));
            initialized = true;
            break;
        } else {
            Log(LogLevel::warning, "Failed to initialize BGFX with renderer: " + std::string(bgfx::getRendererName(r)));
        }
    }

    if (!initialized) {
        Log(LogLevel::error, "BGFX failed to initialize with any renderer.");
        return false;
    }
    
    Log(LogLevel::info, "BGFX initialized with renderer: " + std::string(bgfx::getRendererName(bgfx::getRendererType())));

    // Set debug flags (optional)
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    
    // Set up view 0 with proper clear settings
    bgfx::setViewClear(0,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
        0x303030ff, 1.0f, 0);
    
    // Set view rect for view 0
    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));

    // Set up view and projection matrices using camera component
    float view[16];
    float proj[16];
    
    // During initialization, use default matrices (camera not created yet)
    // Create a view matrix that positions camera back a bit to see the cubes
    glm::mat4 defaultView = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 5.0f),  // Camera position (back from origin)
        glm::vec3(0.0f, 0.0f, -5.0f), // Look at point (where cubes are)
        glm::vec3(0.0f, 1.0f, 0.0f)   // Up vector
    );
    memcpy(view, glm::value_ptr(defaultView), sizeof(float) * 16);
    
    // Create projection matrix
    glm::mat4 projMatrix = GetProjectionMatrix();
    memcpy(proj, glm::value_ptr(projMatrix), sizeof(float) * 16);
    
    // Set the view and projection matrices for view 0
    bgfx::setViewTransform(0, view, proj);

    // Initialize BGFX resources
    InitBGFXResources();

    Log(LogLevel::info, "BGFX initialization complete.");
    return true;
}

// Metaball helper functions (from BGFX 02-metaballs example)
float vertLerp(float* _result, float _iso, uint32_t _idx0, float _v0, uint32_t _idx1, float _v1)
{
	const float* edge0 = s_cube[_idx0];
	const float* edge1 = s_cube[_idx1];

	if (std::abs(_iso-_v1) < 0.00001f)
	{
		_result[0] = edge1[0];
		_result[1] = edge1[1];
		_result[2] = edge1[2];
		return 1.0f;
	}

	if (std::abs(_iso-_v0) < 0.00001f
	||  std::abs(_v0-_v1) < 0.00001f)
	{
		_result[0] = edge0[0];
		_result[1] = edge0[1];
		_result[2] = edge0[2];
		return 0.0f;
	}

	float lerp = (_iso - _v0) / (_v1 - _v0);
	_result[0] = edge0[0] + lerp * (edge1[0] - edge0[0]);
	_result[1] = edge0[1] + lerp * (edge1[1] - edge0[1]);
	_result[2] = edge0[2] + lerp * (edge1[2] - edge0[2]);

	return lerp;
}

uint32_t triangulate(
	  uint8_t* _result
	, uint32_t _stride
	, const float* _rgb
	, const float* _xyz
	, const Grid* _val[8]
	, float _iso
	, float _scale
	)
{
	uint8_t cubeindex = 0;
	cubeindex |= (_val[0]->m_val < _iso) ? 0x01 : 0;
	cubeindex |= (_val[1]->m_val < _iso) ? 0x02 : 0;
	cubeindex |= (_val[2]->m_val < _iso) ? 0x04 : 0;
	cubeindex |= (_val[3]->m_val < _iso) ? 0x08 : 0;
	cubeindex |= (_val[4]->m_val < _iso) ? 0x10 : 0;
	cubeindex |= (_val[5]->m_val < _iso) ? 0x20 : 0;
	cubeindex |= (_val[6]->m_val < _iso) ? 0x40 : 0;
	cubeindex |= (_val[7]->m_val < _iso) ? 0x80 : 0;

	if (0 == s_edges[cubeindex])
	{
		return 0;
	}

	float verts[12][6];
	uint16_t flags = s_edges[cubeindex];

	for (uint32_t ii = 0; ii < 12; ++ii)
	{
		if (flags & (1<<ii) )
		{
			uint32_t idx0 = ii&7;
			uint32_t idx1 = "\x1\x2\x3\x0\x5\x6\x7\x4\x4\x5\x6\x7"[ii];
			float* vertex = verts[ii];
			float lerp = vertLerp(vertex, _iso, idx0, _val[idx0]->m_val, idx1, _val[idx1]->m_val);

			const float* na = _val[idx0]->m_normal;
			const float* nb = _val[idx1]->m_normal;
			vertex[3] = na[0] + lerp * (nb[0] - na[0]);
			vertex[4] = na[1] + lerp * (nb[1] - na[1]);
			vertex[5] = na[2] + lerp * (nb[2] - na[2]);
		}
	}

	const float dr = _rgb[3] - _rgb[0];
	const float dg = _rgb[4] - _rgb[1];
	const float db = _rgb[5] - _rgb[2];

	uint32_t num = 0;
	const int8_t* indices = s_indices[cubeindex];
	for (uint32_t ii = 0; indices[ii] != -1; ++ii)
	{
		const float* vertex = verts[uint8_t(indices[ii])];

		float* xyz = (float*)_result;
		xyz[0] = _xyz[0] + vertex[0]*_scale;
		xyz[1] = _xyz[1] + vertex[1]*_scale;
		xyz[2] = _xyz[2] + vertex[2]*_scale;

		xyz[3] = vertex[3]*_scale;
		xyz[4] = vertex[4]*_scale;
		xyz[5] = vertex[5]*_scale;

		uint32_t rr = uint8_t( (_rgb[0] + vertex[0]*dr)*255.0f);
		uint32_t gg = uint8_t( (_rgb[1] + vertex[1]*dg)*255.0f);
		uint32_t bb = uint8_t( (_rgb[2] + vertex[2]*db)*255.0f);

		uint32_t* abgr = (uint32_t*)&_result[24];
		*abgr = 0xff000000
			  | (bb<<16)
			  | (gg<<8)
			  | rr
			  ;

		_result += _stride;
		++num;
	}

	return num;
}

void InitBGFXResources() {
    // Set up simple vertex layout (position only)
    bgfxVertexLayout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .end();

    // Create vertex buffer
    bgfxVertexBuffer = bgfx::createVertexBuffer(
        bgfx::makeRef(s_triangleVertices, sizeof(s_triangleVertices)),
        bgfxVertexLayout
    );

    // Create index buffer
    bgfxIndexBuffer = bgfx::createIndexBuffer(
        bgfx::makeRef(s_triangleIndices, sizeof(s_triangleIndices))
    );

    // Try to load compiled shaders first
    Log(LogLevel::info, "Attempting to load compiled shaders (.bin)...");
    
    // Try to load vertex shader from compiled file
    FILE* vsFile = fopen("Assets/shaders/test.vs.bin", "rb");
    if (vsFile) {
        fseek(vsFile, 0, SEEK_END);
        long vsSize = ftell(vsFile);
        fseek(vsFile, 0, SEEK_SET);
        
        char* vsSource = new char[vsSize];
        fread(vsSource, 1, vsSize, vsFile);
        fclose(vsFile);
        Log(LogLevel::info, "Loaded vertex shader from file, size: " + std::to_string(vsSize));
        const bgfx::Memory* vsMem = bgfx::alloc((uint32_t)vsSize);
        memcpy(vsMem->data, vsSource, vsSize);
        s_vertexShader = bgfx::createShader(vsMem);
        
        delete[] vsSource;
    } else {
        Log(LogLevel::error, "Could not open compiled vertex shader file: Assets/shaders/test.vs.bin");
    }
    
    // Try to load fragment shader from compiled file
    FILE* fsFile = fopen("Assets/shaders/test.fs.bin", "rb");
    if (fsFile) {
        fseek(fsFile, 0, SEEK_END);
        long fsSize = ftell(fsFile);
        fseek(fsFile, 0, SEEK_SET);
        
        char* fsSource = new char[fsSize];
        fread(fsSource, 1, fsSize, fsFile);
        fclose(fsFile);
        Log(LogLevel::info, "Loaded fragment shader from file, size: " + std::to_string(fsSize));
        const bgfx::Memory* fsMem = bgfx::alloc((uint32_t)fsSize);
        memcpy(fsMem->data, fsSource, fsSize);
        s_fragmentShader = bgfx::createShader(fsMem);
        
        delete[] fsSource;
    } else {
        Log(LogLevel::error, "Could not open compiled fragment shader file: Assets/shaders/test.fs.bin");
    }
    
    // Try to create shader program
    Log(LogLevel::info, "Creating shader program...");
    Log(LogLevel::info, "Vertex shader handle valid: " + std::string(bgfx::isValid(s_vertexShader) ? "Yes" : "No"));
    Log(LogLevel::info, "Fragment shader handle valid: " + std::string(bgfx::isValid(s_fragmentShader) ? "Yes" : "No"));
    
    if (bgfx::isValid(s_vertexShader) && bgfx::isValid(s_fragmentShader)) {
        bgfxSimpleProgram = bgfx::createProgram(s_vertexShader, s_fragmentShader, true);
        Log(LogLevel::info, "BGFX shader program created successfully.");
        Log(LogLevel::info, "Program valid: " + std::string(bgfx::isValid(bgfxSimpleProgram) ? "Yes" : "No"));
    } else {
        Log(LogLevel::error, "Failed to create BGFX shader program.");
    }

    Log(LogLevel::info, "BGFX resources initialized.");
    
    // Initialize cube resources
    InitCubeResources();
    
    // Initialize metaball resources
    InitMetaballResources();
    
    // Initialize raymarch resources
    InitRaymarchResources();
    
    // Initialize mesh resources
    InitMeshResources();
}

bgfx::ShaderHandle CreateShaderFromBinary(const uint8_t* data, uint32_t size, const char* name) {
    // Create a memory reference for the pre-compiled shader binary
    const bgfx::Memory* mem = bgfx::makeRef(data, size);
    
    // Create the shader handle from binary data
    bgfx::ShaderHandle handle = bgfx::createShader(mem);
    
    if (bgfx::isValid(handle)) {
        Log(LogLevel::info, "Created BGFX shader from binary: " + std::string(name));
    } else {
        Log(LogLevel::error, "Failed to create BGFX shader from binary: " + std::string(name));
    }
    
    return handle;
}

void InitCubeResources() {
    // Initialize vertex layout for cubes
    PosColorVertex::init();
    
    // Create cube vertex buffer
    bgfxCubeVertexBuffer = bgfx::createVertexBuffer(
        bgfx::makeRef(s_cubeVertices, sizeof(s_cubeVertices)),
        PosColorVertex::ms_layout
    );
    
    // Create cube index buffer
    bgfxCubeIndexBuffer = bgfx::createIndexBuffer(
        bgfx::makeRef(s_cubeTriList, sizeof(s_cubeTriList))
    );
    
    // Load cube shaders
    Log(LogLevel::info, "Loading cube shaders...");
    
    // Try to load vertex shader from compiled file
    FILE* vsFile = fopen("Assets/shaders/cube_vs.bin", "rb");
    if (vsFile) {
        fseek(vsFile, 0, SEEK_END);
        long vsSize = ftell(vsFile);
        fseek(vsFile, 0, SEEK_SET);
        
        char* vsSource = new char[vsSize];
        fread(vsSource, 1, vsSize, vsFile);
        fclose(vsFile);
        Log(LogLevel::info, "Loaded cube vertex shader from file, size: " + std::to_string(vsSize));
        const bgfx::Memory* vsMem = bgfx::alloc((uint32_t)vsSize);
        memcpy(vsMem->data, vsSource, vsSize);
        bgfx::ShaderHandle cubeVertexShader = bgfx::createShader(vsMem);
        
        delete[] vsSource;
        
        // Try to load fragment shader from compiled file
        FILE* fsFile = fopen("Assets/shaders/cube_fs.bin", "rb");
        if (fsFile) {
            fseek(fsFile, 0, SEEK_END);
            long fsSize = ftell(fsFile);
            fseek(fsFile, 0, SEEK_SET);
            
            char* fsSource = new char[fsSize];
            fread(fsSource, 1, fsSize, fsFile);
            fclose(fsFile);
            Log(LogLevel::info, "Loaded cube fragment shader from file, size: " + std::to_string(fsSize));
            const bgfx::Memory* fsMem = bgfx::alloc((uint32_t)fsSize);
            memcpy(fsMem->data, fsSource, fsSize);
            bgfx::ShaderHandle cubeFragmentShader = bgfx::createShader(fsMem);
            
            delete[] fsSource;
            
            // Create cube shader program
            if (bgfx::isValid(cubeVertexShader) && bgfx::isValid(cubeFragmentShader)) {
                bgfxCubeProgram = bgfx::createProgram(cubeVertexShader, cubeFragmentShader, true);
                Log(LogLevel::info, "Cube shader program created successfully.");
            } else {
                Log(LogLevel::error, "Failed to create cube shader program.");
            }
        } else {
            Log(LogLevel::error, "Could not open compiled cube fragment shader file: Assets/shaders/cube_fs.bin");
        }
    } else {
        Log(LogLevel::error, "Could not open compiled cube vertex shader file: Assets/shaders/cube_vs.bin");
    }
    
    // Create some default cubes with better spacing
    CreateCube(glm::vec3(0.0f, 0.0f, -5.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    CreateCube(glm::vec3(4.0f, 0.0f, -5.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    CreateCube(glm::vec3(-4.0f, 0.0f, -5.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    
    Log(LogLevel::info, "Cube resources initialized.");
}

void CreateCube(const glm::vec3& position, const glm::vec4& color) {
    C6GE::CubeComponent cube;
    cube.position = position;
    cube.color = color;
    cube.rotationSpeed = 1.0f;
    g_cubes.push_back(cube);
    Log(LogLevel::info, "Created cube at position: " + std::to_string(position.x) + ", " + 
        std::to_string(position.y) + ", " + std::to_string(position.z));
}

void RenderCubes() {
    if (!bgfx::isValid(bgfxCubeProgram)) {
        bgfx::dbgTextPrintf(0, 8, 0x0c, "Cube shader not available!");
        return;
    }
    
    // Set render states for cubes - fix face culling for BGFX
    uint64_t state = 0
        | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_WRITE_Z
        | BGFX_STATE_DEPTH_TEST_LESS
        | BGFX_STATE_CULL_CCW  // Changed from CW to CCW for BGFX
        | BGFX_STATE_MSAA;
    
    // Render each cube
    for (auto& cube : g_cubes) {
        // Update cube rotation
        cube.rotation.y += cube.rotationSpeed;
        
        // Get transform matrix
        glm::mat4 modelMatrix = cube.GetTransformMatrix();
        float mtx[16];
        memcpy(mtx, glm::value_ptr(modelMatrix), sizeof(float) * 16);
        
        // Set transform matrix
        bgfx::setTransform(mtx);
        
        // Set vertex and index buffers for each cube
        bgfx::setVertexBuffer(0, bgfxCubeVertexBuffer);
        bgfx::setIndexBuffer(bgfxCubeIndexBuffer);
        
        // Set render state
        bgfx::setState(state);
        
        // Submit for rendering
        bgfx::submit(0, bgfxCubeProgram);
    }
}

void renderScreenSpaceQuad(uint8_t _view, bgfx::ProgramHandle _program, float _x, float _y, float _width, float _height)
{
    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;

    if (bgfx::allocTransientBuffers(&tvb, PosColorTexCoord0Vertex::ms_layout, 4, &tib, 6) )
    {
        PosColorTexCoord0Vertex* vertex = (PosColorTexCoord0Vertex*)tvb.data;

        float zz = 0.0f;

        const float minx = _x;
        const float maxx = _x + _width;
        const float miny = _y;
        const float maxy = _y + _height;

        float minu = -1.0f;
        float minv = -1.0f;
        float maxu =  1.0f;
        float maxv =  1.0f;

        vertex[0].m_x = minx;
        vertex[0].m_y = miny;
        vertex[0].m_z = zz;
        vertex[0].m_abgr = 0xff0000ff;
        vertex[0].m_u = minu;
        vertex[0].m_v = minv;

        vertex[1].m_x = maxx;
        vertex[1].m_y = miny;
        vertex[1].m_z = zz;
        vertex[1].m_abgr = 0xff00ff00;
        vertex[1].m_u = maxu;
        vertex[1].m_v = minv;

        vertex[2].m_x = maxx;
        vertex[2].m_y = maxy;
        vertex[2].m_z = zz;
        vertex[2].m_abgr = 0xffff0000;
        vertex[2].m_u = maxu;
        vertex[2].m_v = maxv;

        vertex[3].m_x = minx;
        vertex[3].m_y = maxy;
        vertex[3].m_z = zz;
        vertex[3].m_abgr = 0xffffffff;
        vertex[3].m_u = minu;
        vertex[3].m_v = maxv;

        uint16_t* indices = (uint16_t*)tib.data;

        indices[0] = 0;
        indices[1] = 2;
        indices[2] = 1;
        indices[3] = 0;
        indices[4] = 3;
        indices[5] = 2;

        bgfx::setState(BGFX_STATE_DEFAULT);
        bgfx::setIndexBuffer(&tib);
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::submit(_view, _program);
    }
}

void InitMetaballResources() {
    // Initialize metaball vertex layout
    PosNormalColorVertex::init();
    
    // Create metaball shader program
    bgfx::RendererType::Enum type = bgfx::getRendererType();
    
    // Try to load metaball shaders
    FILE* vsFile = fopen("Assets/shaders/metaball_vs.bin", "rb");
    FILE* fsFile = fopen("Assets/shaders/metaball_fs.bin", "rb");
    
    if (!vsFile) {
        Log(LogLevel::error, "Could not open metaball vertex shader: Assets/shaders/metaball_vs.bin");
    }
    if (!fsFile) {
        Log(LogLevel::error, "Could not open metaball fragment shader: Assets/shaders/metaball_fs.bin");
    }
    
    bgfx::ShaderHandle vsh = BGFX_INVALID_HANDLE;
    bgfx::ShaderHandle fsh = BGFX_INVALID_HANDLE;
    
    if (vsFile && fsFile) {
        // Load vertex shader
        fseek(vsFile, 0, SEEK_END);
        long vsSize = ftell(vsFile);
        fseek(vsFile, 0, SEEK_SET);
        char* vsSource = new char[vsSize];
        fread(vsSource, 1, vsSize, vsFile);
        fclose(vsFile);
        
        const bgfx::Memory* vsMem = bgfx::alloc((uint32_t)vsSize);
        memcpy(vsMem->data, vsSource, vsSize);
        vsh = bgfx::createShader(vsMem);
        delete[] vsSource;
        
        // Load fragment shader
        fseek(fsFile, 0, SEEK_END);
        long fsSize = ftell(fsFile);
        fseek(fsFile, 0, SEEK_SET);
        char* fsSource = new char[fsSize];
        fread(fsSource, 1, fsSize, fsFile);
        fclose(fsFile);
        
        const bgfx::Memory* fsMem = bgfx::alloc((uint32_t)fsSize);
        memcpy(fsMem->data, fsSource, fsSize);
        fsh = bgfx::createShader(fsMem);
        delete[] fsSource;
        
        // Create program
        if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
            bgfxMetaballProgram = bgfx::createProgram(vsh, fsh, true);
            Log(LogLevel::info, "Metaball shaders loaded successfully.");
        }
    } else {
        Log(LogLevel::error, "Could not load metaball shaders, using fallback.");
        // Use cube shader as fallback
        bgfxMetaballProgram = bgfxCubeProgram;
    }
    
    // Initialize metaball grid
    g_metaballGrid = new Grid[kMaxDims*kMaxDims*kMaxDims];
    if (!g_metaballGrid) {
        Log(LogLevel::error, "Failed to allocate metaball grid!");
        return;
    }
    
    // Initialize grid values to zero
    for (int i = 0; i < kMaxDims*kMaxDims*kMaxDims; ++i) {
        g_metaballGrid[i].m_val = 0.0f;
        g_metaballGrid[i].m_normal[0] = 0.0f;
        g_metaballGrid[i].m_normal[1] = 0.0f;
        g_metaballGrid[i].m_normal[2] = 0.0f;
    }
    
    g_metaballTimeOffset = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    
    Log(LogLevel::info, "Metaball resources initialized.");
}

void InitRaymarchResources() {
    Log(LogLevel::info, "Initializing raymarch resources...");
    
    // Initialize vertex layout
    PosColorTexCoord0Vertex::init();
    
    // Create raymarch shader program
    bgfx::RendererType::Enum type = bgfx::getRendererType();
    
    // Try to load raymarch shaders
    FILE* vsFile = fopen("Assets/shaders/raymarch_vs.bin", "rb");
    FILE* fsFile = fopen("Assets/shaders/raymarch_fs.bin", "rb");
    
    if (!vsFile) {
        Log(LogLevel::error, "Could not open raymarch vertex shader: Assets/shaders/raymarch_vs.bin");
    }
    if (!fsFile) {
        Log(LogLevel::error, "Could not open raymarch fragment shader: Assets/shaders/raymarch_fs.bin");
    }
    
    bgfx::ShaderHandle vsh = BGFX_INVALID_HANDLE;
    bgfx::ShaderHandle fsh = BGFX_INVALID_HANDLE;
    
    if (vsFile && fsFile) {
        // Load vertex shader
        fseek(vsFile, 0, SEEK_END);
        long vsSize = ftell(vsFile);
        fseek(vsFile, 0, SEEK_SET);
        char* vsSource = new char[vsSize];
        fread(vsSource, 1, vsSize, vsFile);
        fclose(vsFile);
        
        const bgfx::Memory* vsMem = bgfx::alloc((uint32_t)vsSize);
        memcpy(vsMem->data, vsSource, vsSize);
        vsh = bgfx::createShader(vsMem);
        delete[] vsSource;
        
        // Load fragment shader
        fseek(fsFile, 0, SEEK_END);
        long fsSize = ftell(fsFile);
        fseek(fsFile, 0, SEEK_SET);
        char* fsSource = new char[fsSize];
        fread(fsSource, 1, fsSize, fsFile);
        fclose(fsFile);
        
        const bgfx::Memory* fsMem = bgfx::alloc((uint32_t)fsSize);
        memcpy(fsMem->data, fsSource, fsSize);
        fsh = bgfx::createShader(fsMem);
        delete[] fsSource;
        
        // Create program
        if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
            bgfxRaymarchProgram = bgfx::createProgram(vsh, fsh, true);
            Log(LogLevel::info, "Raymarch shaders loaded successfully.");
        }
    } else {
        Log(LogLevel::error, "Could not load raymarch shaders, using fallback.");
        // Use metaball shader as fallback
        bgfxRaymarchProgram = bgfxMetaballProgram;
    }
    
    // Create uniforms
    bgfxRaymarchMtx = bgfx::createUniform("u_mtx", bgfx::UniformType::Mat4);
    bgfxRaymarchLightDirTime = bgfx::createUniform("u_lightDirTime", bgfx::UniformType::Vec4);
    
    g_raymarchTimeOffset = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    
    Log(LogLevel::info, "Raymarch resources initialized.");
}

void InitMeshResources() {
    Log(LogLevel::info, "Initializing mesh resources...");
    
    // Initialize vertex layout
    PosNormalTexCoordVertex::init();
    
    // Create mesh shader program
    bgfx::RendererType::Enum type = bgfx::getRendererType();
    
    // Try to load mesh shaders
    FILE* vsFile = fopen("Assets/shaders/mesh_vs.bin", "rb");
    FILE* fsFile = fopen("Assets/shaders/mesh_fs.bin", "rb");
    
    if (!vsFile) {
        Log(LogLevel::error, "Could not open mesh vertex shader: Assets/shaders/mesh_vs.bin");
    }
    if (!fsFile) {
        Log(LogLevel::error, "Could not open mesh fragment shader: Assets/shaders/mesh_fs.bin");
    }
    
    bgfx::ShaderHandle vsh = BGFX_INVALID_HANDLE;
    bgfx::ShaderHandle fsh = BGFX_INVALID_HANDLE;
    
    if (vsFile && fsFile) {
        // Load vertex shader
        fseek(vsFile, 0, SEEK_END);
        long vsSize = ftell(vsFile);
        fseek(vsFile, 0, SEEK_SET);
        char* vsSource = new char[vsSize];
        fread(vsSource, 1, vsSize, vsFile);
        fclose(vsFile);
        
        const bgfx::Memory* vsMem = bgfx::alloc((uint32_t)vsSize);
        memcpy(vsMem->data, vsSource, vsSize);
        vsh = bgfx::createShader(vsMem);
        delete[] vsSource;
        
        // Load fragment shader
        fseek(fsFile, 0, SEEK_END);
        long fsSize = ftell(fsFile);
        fseek(fsFile, 0, SEEK_SET);
        char* fsSource = new char[fsSize];
        fread(fsSource, 1, fsSize, fsFile);
        fclose(fsFile);
        
        const bgfx::Memory* fsMem = bgfx::alloc((uint32_t)fsSize);
        memcpy(fsMem->data, fsSource, fsSize);
        fsh = bgfx::createShader(fsMem);
        delete[] fsSource;
        
        // Create program
        if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
            bgfxMeshProgram = bgfx::createProgram(vsh, fsh, true);
            Log(LogLevel::info, "Mesh shaders loaded successfully.");
        }
    } else {
        Log(LogLevel::error, "Could not load mesh shaders, using fallback.");
        // Use raymarch shader as fallback
        bgfxMeshProgram = bgfxRaymarchProgram;
    }
    
    // Create uniforms
    bgfxMeshMtx = bgfx::createUniform("u_mtx", bgfx::UniformType::Mat4);
    bgfxMeshLightDirTime = bgfx::createUniform("u_lightDirTime", bgfx::UniformType::Vec4);
    
    g_meshTimeOffset = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    
    Log(LogLevel::info, "Mesh resources initialized.");
}

// ---- HDR helpers ----
static void updateHdrFramebuffer(uint16_t width, uint16_t height) {
    if (width == 0 || height == 0) return;
    // Only update if size changed or framebuffer is invalid
    if (bgfx::isValid(g_hdrFb) && width == g_backWidth && height == g_backHeight) {
        return;
    }
    
    Log(LogLevel::info, "Updating HDR framebuffer from " + std::to_string(g_backWidth) + "x" + std::to_string(g_backHeight) + " to " + std::to_string(width) + "x" + std::to_string(height));
    if (bgfx::isValid(g_hdrFb)) {
        bgfx::destroy(g_hdrFb);
        g_hdrFb = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(g_hdrColor)) {
        bgfx::destroy(g_hdrColor);
        g_hdrColor = BGFX_INVALID_HANDLE;
    }
    const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    g_hdrColor = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA16F, flags);
    
    if (!bgfx::isValid(g_hdrColor)) {
        Log(LogLevel::error, "Failed to create HDR color texture");
        return;
    }
    
    bgfx::Attachment att; att.init(g_hdrColor);
    g_hdrFb = bgfx::createFrameBuffer(1, &att, true);
    
    if (!bgfx::isValid(g_hdrFb)) {
        Log(LogLevel::error, "Failed to create HDR framebuffer");
        return;
    }
    
    g_backWidth = width; g_backHeight = height;
    Log(LogLevel::info, "HDR framebuffer updated to " + std::to_string(width) + "x" + std::to_string(height));
    if (!bgfx::isValid(u_exposure)) {
        u_exposure = bgfx::createUniform("u_exposure", bgfx::UniformType::Vec4);
    }
    if (!bgfx::isValid(bgfxTonemapProgram)) {
        FILE* vsFile = fopen("Assets/shaders/hdr_tonemap_vs.bin", "rb");
        FILE* fsFile = fopen("Assets/shaders/hdr_tonemap_fs.bin", "rb");
        if (vsFile && fsFile) {
            fseek(vsFile, 0, SEEK_END); long vsSize = ftell(vsFile); fseek(vsFile, 0, SEEK_SET);
            char* vsSrc = new char[vsSize]; fread(vsSrc, 1, vsSize, vsFile); fclose(vsFile);
            const bgfx::Memory* vmem = bgfx::alloc((uint32_t)vsSize); memcpy(vmem->data, vsSrc, vsSize); delete[] vsSrc;
            bgfx::ShaderHandle vsh = bgfx::createShader(vmem);
            fseek(fsFile, 0, SEEK_END); long fsSize = ftell(fsFile); fseek(fsFile, 0, SEEK_SET);
            char* fsSrc = new char[fsSize]; fread(fsSrc, 1, fsSize, fsFile); fclose(fsFile);
            const bgfx::Memory* fmem = bgfx::alloc((uint32_t)fsSize); memcpy(fmem->data, fsSrc, fsSize); delete[] fsSrc;
            bgfx::ShaderHandle fsh = bgfx::createShader(fmem);
            if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
                bgfxTonemapProgram = bgfx::createProgram(vsh, fsh, true);
            }
        }
    }
}

void BeginHDR() {
    GLFWwindow* window = glfwGetCurrentContext();
    int ww = 0, hh = 0; if (window) glfwGetWindowSize(window, &ww, &hh);
    updateHdrFramebuffer((uint16_t)ww, (uint16_t)hh);
    // Render the scene into view 0 bound to HDR framebuffer
    if (bgfx::isValid(g_hdrFb)) {
        bgfx::setViewFrameBuffer(0, g_hdrFb);
        bgfx::setViewRect(0, 0, 0, (uint16_t)ww, (uint16_t)hh);
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00000000, 1.0f, 0);
        bgfx::touch(0);
    }
}

void EndHDRAndTonemap() {
    if (!bgfx::isValid(bgfxTonemapProgram) || !bgfx::isValid(g_hdrColor)) return;
    static bgfx::UniformHandle s_hdrColor = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(s_hdrColor)) {
        s_hdrColor = bgfx::createUniform("s_hdrColor", bgfx::UniformType::Sampler);
    }
    GLFWwindow* window = glfwGetCurrentContext(); int ww=0, hh=0; if (window) glfwGetWindowSize(window, &ww, &hh);
    
    // Set view 2 to render to the backbuffer (not the HDR framebuffer)
    bgfx::setViewFrameBuffer(2, BGFX_INVALID_HANDLE);
    bgfx::setViewRect(2, 0, 0, (uint16_t)ww, (uint16_t)hh);
    bgfx::setViewClear(2, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::touch(2);
    const float exposure[4] = {1.2f,0,0,0};
    bgfx::setUniform(u_exposure, exposure);
    bgfx::setTexture(0, s_hdrColor, g_hdrColor);
    renderScreenSpaceQuad(2, bgfxTonemapProgram, 0.0f, 0.0f, (float)ww, (float)hh);
}

void RenderMetaballs() {
    Log(LogLevel::info, "RenderMetaballs: Starting...");
    
    if (!bgfx::isValid(bgfxMetaballProgram)) {
        Log(LogLevel::error, "RenderMetaballs: Metaball shader not available!");
        bgfx::dbgTextPrintf(0, 12, 0x0c, "Metaball shader not available!");
        return;
    }
    
    Log(LogLevel::info, "RenderMetaballs: Shader is valid, proceeding...");
    
    const uint32_t ypitch = kMaxDims;
    const uint32_t zpitch = kMaxDims*kMaxDims;
    
    Log(LogLevel::info, "RenderMetaballs: Setting up matrices...");
    
    // Get camera component for view matrix
    auto* camera = GetComponent<CameraComponent>("camera");
    if (!camera) {
        Log(LogLevel::error, "RenderMetaballs: Camera component not found!");
        return;
    }
    
    // Set view and projection matrices using camera
        glm::mat4 viewMatrix = GetViewMatrix(*camera);
    float aspectRatio = GetWindowAspectRatio();
    glm::mat4 projMatrix = glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 100.0f);
    
        float view[16];
    float proj[16];
        memcpy(view, glm::value_ptr(viewMatrix), sizeof(float) * 16);
    memcpy(proj, glm::value_ptr(projMatrix), sizeof(float) * 16);
    bgfx::setViewTransform(0, view, proj);
    
    // Set model matrix (identity)
    float model[16];
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    memcpy(model, glm::value_ptr(modelMatrix), sizeof(float) * 16);
    
    // Set uniforms for metaball shader - use existing uniforms from BGFX
    // The shader should use the view/projection matrices set via setViewTransform
    // and we'll set the model matrix as identity
    
    Log(LogLevel::info, "RenderMetaballs: Matrices set successfully...");
    
    Log(LogLevel::info, "RenderMetaballs: Calculating time...");
    
    // Get time
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    float time = (float)((now - g_metaballTimeOffset) / 1000000000.0); // Convert to seconds
    
    Log(LogLevel::info, "RenderMetaballs: Creating animated spheres...");
    
    // Create animated spheres
    const uint32_t numSpheres = 16;
    float sphere[numSpheres][4];
    for (uint32_t ii = 0; ii < numSpheres; ++ii)
    {
        sphere[ii][0] = std::sin(time*(ii*0.21f)+ii*0.37f) * (kMaxDimsF * 0.5f - 8.0f);
        sphere[ii][1] = std::sin(time*(ii*0.37f)+ii*0.67f) * (kMaxDimsF * 0.5f - 8.0f);
        sphere[ii][2] = std::cos(time*(ii*0.11f)+ii*0.13f) * (kMaxDimsF * 0.5f - 8.0f);
        sphere[ii][3] = 1.0f/(3.0f + (std::sin(time*(ii*0.13f) )*0.5f+0.5f)*0.9f );
    }
    
    Log(LogLevel::info, "RenderMetaballs: Spheres created successfully...");
    
    Log(LogLevel::info, "RenderMetaballs: Starting metaball field update...");
    
    // Check if grid is valid
    if (!g_metaballGrid) {
        Log(LogLevel::error, "RenderMetaballs: Metaball grid is null!");
        return;
    }
    
    // Update metaball field - use smaller resolution for better performance
    uint32_t numDims = 16; // Reduced from kMaxDims for better performance
    const float numDimsF = float(numDims);
    const float scale    = kMaxDimsF/numDimsF;
    
    for (uint32_t zz = 0; zz < numDims; ++zz)
    {
        for (uint32_t yy = 0; yy < numDims; ++yy)
        {
            uint32_t offset = (zz*kMaxDims+yy)*kMaxDims;
            
            for (uint32_t xx = 0; xx < numDims; ++xx)
            {
                uint32_t xoffset = offset + xx;
                
                float dist = 0.0f;
                float prod = 1.0f;
                for (uint32_t ii = 0; ii < numSpheres; ++ii)
                {
                    const float* pos = sphere[ii];
                    float dx   = pos[0] - (-kMaxDimsF*0.5f + float(xx)*scale);
                    float dy   = pos[1] - (-kMaxDimsF*0.5f + float(yy)*scale);
                    float dz   = pos[2] - (-kMaxDimsF*0.5f + float(zz)*scale);
                    float invR = pos[3];
                    float dot  = dx*dx + dy*dy + dz*dz;
                    dot *= invR * invR;
                    
                    dist *= dot;
                    dist += prod;
                    prod *= dot;
                }
                
                g_metaballGrid[xoffset].m_val = dist / prod - 1.0f;
            }
        }
    }
    
    // Calculate normals
    for (uint32_t zz = 1; zz < numDims-1; ++zz)
    {
        for (uint32_t yy = 1; yy < numDims-1; ++yy)
        {
            uint32_t offset = (zz*kMaxDims+yy)*kMaxDims;
            
            for (uint32_t xx = 1; xx < numDims-1; ++xx)
            {
                uint32_t xoffset = offset + xx;
                
                Grid* grid = g_metaballGrid;
                glm::vec3 normal(
                    grid[xoffset-1     ].m_val - grid[xoffset+1     ].m_val,
                    grid[xoffset-ypitch].m_val - grid[xoffset+ypitch].m_val,
                    grid[xoffset-zpitch].m_val - grid[xoffset+zpitch].m_val
                );
                
                normal = glm::normalize(normal);
                grid[xoffset].m_normal[0] = normal.x;
                grid[xoffset].m_normal[1] = normal.y;
                grid[xoffset].m_normal[2] = normal.z;
            }
        }
    }
    
    Log(LogLevel::info, "RenderMetaballs: Allocating transient vertex buffer...");
    
    // Allocate transient vertex buffer
    uint32_t maxVertices = (32<<10);
    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, maxVertices, PosNormalColorVertex::ms_layout);
    
    Log(LogLevel::info, "RenderMetaballs: Vertex buffer allocated...");
    
    PosNormalColorVertex* vertex = (PosNormalColorVertex*)tvb.data;
    uint32_t numVertices = 0;
    const float invDim = 1.0f/(numDimsF-1.0f);
    
    // Simple fallback: render a basic sphere if triangulation fails
    bool triangulationSuccess = false;
    
    // Simple metaball rendering - render animated spheres
    Log(LogLevel::info, "RenderMetaballs: Using simple sphere rendering...");
    
    // Create animated spheres based on the metaball field
    for (uint32_t ii = 0; ii < 16 && numVertices + 36 < maxVertices; ++ii) {
        float x = sphere[ii][0];
        float y = sphere[ii][1];
        float z = sphere[ii][2];
        float radius = 1.0f / sphere[ii][3];
        
        // Create a simple sphere mesh for each metaball
        const uint32_t segments = 8;
        const uint32_t rings = 6;
        
        for (uint32_t ring = 0; ring < rings - 1; ++ring) {
            for (uint32_t segment = 0; segment < segments; ++segment) {
                if (numVertices + 6 >= maxVertices) break;
                
                float phi1 = (float(ring) / float(rings - 1)) * 3.14159f;
                float phi2 = (float(ring + 1) / float(rings - 1)) * 3.14159f;
                float theta1 = (float(segment) / float(segments)) * 2.0f * 3.14159f;
                float theta2 = (float(segment + 1) / float(segments)) * 2.0f * 3.14159f;
                
                // Create two triangles for this quad
                for (int tri = 0; tri < 2; ++tri) {
                    float phi[3], theta[3];
                    if (tri == 0) {
                        phi[0] = phi1; theta[0] = theta1;
                        phi[1] = phi2; theta[1] = theta1;
                        phi[2] = phi1; theta[2] = theta2;
                    } else {
                        phi[0] = phi2; theta[0] = theta1;
                        phi[1] = phi2; theta[1] = theta2;
                        phi[2] = phi1; theta[2] = theta2;
                    }
                    
                    for (int i = 0; i < 3; ++i) {
                        float nx = std::sin(phi[i]) * std::cos(theta[i]);
                        float ny = std::sin(phi[i]) * std::sin(theta[i]);
                        float nz = std::cos(phi[i]);
                        
                        vertex->m_pos[0] = x + nx * radius;
                        vertex->m_pos[1] = y + ny * radius;
                        vertex->m_pos[2] = z + nz * radius;
                        vertex->m_normal[0] = nx;
                        vertex->m_normal[1] = ny;
                        vertex->m_normal[2] = nz;
                        vertex->m_abgr = 0xff0000ff; // Red color
                        vertex++;
                        numVertices++;
                    }
                }
            }
        }
    }
    
    Log(LogLevel::info, "RenderMetaballs: Setting render states...");
    
    // Set render states
    bgfx::setState(BGFX_STATE_DEFAULT);
    
    Log(LogLevel::info, "RenderMetaballs: Setting vertex buffer...");
    
    // Set vertex buffer
    bgfx::setVertexBuffer(0, &tvb, 0, numVertices);
    
    Log(LogLevel::info, "RenderMetaballs: Submitting for rendering...");
    
    // Submit for rendering
    if (numVertices > 0) {
        bgfx::submit(0, bgfxMetaballProgram);
        Log(LogLevel::info, "RenderMetaballs: Metaballs rendered successfully!");
    } else {
        Log(LogLevel::warning, "RenderMetaballs: No vertices generated, skipping render");
    }
    
    Log(LogLevel::info, "RenderMetaballs: Rendering completed successfully!");
    }

void RenderRaymarch() {
    Log(LogLevel::info, "RenderRaymarch: Starting...");
    
    if (!bgfx::isValid(bgfxRaymarchProgram)) {
        Log(LogLevel::error, "RenderRaymarch: Raymarch shader not available!");
        bgfx::dbgTextPrintf(0, 14, 0x0c, "Raymarch shader not available!");
        return;
    }
    
    Log(LogLevel::info, "RenderRaymarch: Shader is valid, proceeding...");
    
    // Get camera component for view matrix
    auto* camera = GetComponent<CameraComponent>("camera");
    if (!camera) {
        Log(LogLevel::error, "RenderRaymarch: Camera component not found!");
        return;
    }
    
    // Set view and projection matrices using camera
    glm::mat4 viewMatrix = GetViewMatrix(*camera);
    float aspectRatio = GetWindowAspectRatio();
    glm::mat4 projMatrix = glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 100.0f);
    
    float view[16];
        float proj[16];
    memcpy(view, glm::value_ptr(viewMatrix), sizeof(float) * 16);
        memcpy(proj, glm::value_ptr(projMatrix), sizeof(float) * 16);
        
    // Set view and projection matrix for view 0
        bgfx::setViewTransform(0, view, proj);
        
    // Create orthographic projection for screen space quad
    const bgfx::Caps* caps = bgfx::getCaps();
    float ortho[16];
    GLFWwindow* window = glfwGetCurrentContext();
    int width = 1280, height = 720;
    if (window) {
        glfwGetWindowSize(window, &width, &height);
    }
    glm::mat4 orthoMatrix = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 0.0f, 100.0f);
    memcpy(ortho, glm::value_ptr(orthoMatrix), sizeof(float) * 16);
    
    // Set view and projection matrix for view 1
    bgfx::setViewTransform(1, NULL, ortho);
    
    // Calculate time
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    float time = (float)((now - g_raymarchTimeOffset) / 1000000000.0); // Convert to seconds
    
    // Calculate matrices
    float vp[16];
    glm::mat4 vpMatrix = projMatrix * viewMatrix;
    memcpy(vp, glm::value_ptr(vpMatrix), sizeof(float) * 16);
    
    // Create rotation matrix
    float mtx[16];
    glm::mat4 rotMatrix = glm::rotate(glm::mat4(1.0f), time, glm::vec3(1.0f, 0.0f, 0.0f)) * 
                          glm::rotate(glm::mat4(1.0f), time * 0.37f, glm::vec3(0.0f, 1.0f, 0.0f));
    memcpy(mtx, glm::value_ptr(rotMatrix), sizeof(float) * 16);
    
    // Calculate inverse matrices
    float mtxInv[16];
    glm::mat4 mtxInvMatrix = glm::inverse(rotMatrix);
    memcpy(mtxInv, glm::value_ptr(mtxInvMatrix), sizeof(float) * 16);
    
    // Calculate light direction
    float lightDirTime[4];
    glm::vec3 lightDirModelN = glm::normalize(glm::vec3(-0.4f, -0.5f, -1.0f));
    glm::vec3 lightDirWorld = mtxInvMatrix * glm::vec4(lightDirModelN, 0.0f);
    lightDirTime[0] = lightDirWorld.x;
    lightDirTime[1] = lightDirWorld.y;
    lightDirTime[2] = lightDirWorld.z;
    lightDirTime[3] = time;
    bgfx::setUniform(bgfxRaymarchLightDirTime, lightDirTime);
    
    // Calculate MVP and inverse
    float mvp[16];
    glm::mat4 mvpMatrix = rotMatrix * vpMatrix;
    memcpy(mvp, glm::value_ptr(mvpMatrix), sizeof(float) * 16);
    
    float invMvp[16];
    glm::mat4 invMvpMatrix = glm::inverse(mvpMatrix);
    memcpy(invMvp, glm::value_ptr(invMvpMatrix), sizeof(float) * 16);
    bgfx::setUniform(bgfxRaymarchMtx, invMvp);
    
    // Render screen space quad
    renderScreenSpaceQuad(1, bgfxRaymarchProgram, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    
    Log(LogLevel::info, "RenderRaymarch: Rendering completed successfully!");
}

void RenderMesh() {
    Log(LogLevel::info, "RenderMesh: This function is deprecated - use RenderBGFXObject instead");
}

void RenderBGFXObject(const std::string& objectName) {
    Log(LogLevel::info, "RenderBGFXObject: Rendering object '" + objectName + "'");
    
    if (!bgfx::isValid(bgfxMeshProgram)) {
        Log(LogLevel::error, "RenderBGFXObject: Mesh shader not available!");
        return;
    }
    
    // Get the model component for this object
    auto* modelComponent = GetComponent<ModelComponent>(objectName);
    if (!modelComponent) {
        Log(LogLevel::error, "RenderBGFXObject: ModelComponent not found for object '" + objectName + "'");
        return;
    }
    
    if (!modelComponent->loaded || !modelComponent->mesh.loaded) {
        Log(LogLevel::error, "RenderBGFXObject: Model not loaded for object '" + objectName + "'");
        return;
    }
    
    // Get camera component for view matrix
    auto* camera = GetComponent<CameraComponent>("camera");
    if (!camera) {
        Log(LogLevel::error, "RenderBGFXObject: Camera component not found!");
        return;
    }
    
    // Get view and projection matrices using camera
    glm::mat4 viewMatrix = GetViewMatrix(*camera);
    float aspectRatio = GetWindowAspectRatio();
    glm::mat4 projMatrix = glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 100.0f);
    
    // Get transform component for this object
    auto* transformComponent = GetComponent<TransformComponent>(objectName);
    glm::mat4 modelMatrix;
    
    if (transformComponent) {
        // Use the transform component to create the model matrix
        modelMatrix = transformComponent->GetModelMatrix();
        
        // Debug: Log the transform values
        Log(LogLevel::info, "RenderBGFXObject: Transform - Pos(" + 
            std::to_string(transformComponent->Position.x) + ", " +
            std::to_string(transformComponent->Position.y) + ", " +
            std::to_string(transformComponent->Position.z) + ") Rot(" +
            std::to_string(transformComponent->Rotation.x) + ", " +
            std::to_string(transformComponent->Rotation.y) + ", " +
            std::to_string(transformComponent->Rotation.z) + ") Scale(" +
            std::to_string(transformComponent->Scale.x) + ", " +
            std::to_string(transformComponent->Scale.y) + ", " +
            std::to_string(transformComponent->Scale.z) + ")");
    } else {
        // Fallback to identity matrix if no transform component
        Log(LogLevel::warning, "RenderBGFXObject: No TransformComponent found for object '" + objectName + "', using identity matrix");
        modelMatrix = glm::mat4(1.0f);
    }
    
    // Set BGFX view/projection and model transforms so shader receives u_modelViewProj
    float view[16];
    float proj[16];
    float model[16];
    memcpy(view, glm::value_ptr(viewMatrix), sizeof(float) * 16);
    memcpy(proj, glm::value_ptr(projMatrix), sizeof(float) * 16);
    memcpy(model, glm::value_ptr(modelMatrix), sizeof(float) * 16);
    bgfx::setViewTransform(0, view, proj);
    bgfx::setTransform(model);
    
    // Set light direction and time
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    float meshTime = (float)((now - g_meshTimeOffset) / 1000000000.0); // Convert to seconds
    glm::vec4 lightDirTime(0.0f, 0.0f, -1.0f, meshTime);
    bgfx::setUniform(bgfxMeshLightDirTime, glm::value_ptr(lightDirTime));
    
    // Set render states - disable face culling to show all faces
    bgfx::setState(BGFX_STATE_DEFAULT & ~BGFX_STATE_CULL_MASK);
    
    // Bind diffuse texture from TextureComponent if present
    static bgfx::UniformHandle s_diffuse = BGFX_INVALID_HANDLE;
    static bgfx::UniformHandle s_roughness = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(s_diffuse)) {
        s_diffuse = bgfx::createUniform("s_diffuse", bgfx::UniformType::Sampler);
    }
    if (!bgfx::isValid(s_roughness)) {
        s_roughness = bgfx::createUniform("s_roughness", bgfx::UniformType::Sampler);
    }
    if (auto* tex = GetComponent<TextureComponent>(objectName)) {
        if (!tex->loaded) tex->Load();
        if (tex->loaded && bgfx::isValid(tex->diffuse)) {
            bgfx::setTexture(0, s_diffuse, tex->diffuse);
        }
        if (tex->loaded && bgfx::isValid(tex->roughness)) {
            bgfx::setTexture(1, s_roughness, tex->roughness);
        }
    } else {
        // Clear texture bindings for objects without textures
        bgfx::setTexture(0, s_diffuse, BGFX_INVALID_HANDLE);
        bgfx::setTexture(1, s_roughness, BGFX_INVALID_HANDLE);
    }

    // Submit mesh for rendering
    bgfx::setVertexBuffer(0, modelComponent->mesh.vb);
    bgfx::setIndexBuffer(modelComponent->mesh.ib);
    bgfx::submit(0, bgfxMeshProgram);
    
    Log(LogLevel::info, "RenderBGFXObject: Object '" + objectName + "' rendered successfully!");
}

void RenderBGFXObjectsInstanced(const std::vector<std::string>& objectNames) {
    if (!bgfx::isValid(bgfxMeshProgram) || objectNames.empty()) return;

    // Assume all objects share the same model buffers (table model)
    auto* firstModel = GetComponent<ModelComponent>(objectNames[0]);
    if (!firstModel || !firstModel->loaded || !firstModel->mesh.loaded) return;

    // Gather instance transforms
    const uint32_t requested = static_cast<uint32_t>(objectNames.size());

    std::vector<float> instanceData;
    instanceData.reserve(requested * 4); // 4 floats per instance (x,y,z,w)

    for (uint32_t i = 0; i < requested; ++i) {
        auto* tr = GetComponent<TransformComponent>(objectNames[i]);
        glm::vec3 pos = tr ? tr->Position : glm::vec3(0.0f);
        instanceData.push_back(pos.x);
        instanceData.push_back(pos.y);
        instanceData.push_back(pos.z);
        instanceData.push_back(1.0f); // w component
    }

    // Upload instance data as vec4 per instance
    const uint32_t stride = sizeof(float) * 4;
    const uint32_t avail = bgfx::getAvailInstanceDataBuffer(requested, stride);
    if (avail == 0) return;
    bgfx::InstanceDataBuffer idb;
    bgfx::allocInstanceDataBuffer(&idb, avail, stride);
    memcpy(idb.data, instanceData.data(), avail * stride);

        // Set shared view/proj from camera
        auto* camera = GetComponent<CameraComponent>("camera");
        if (!camera) return;
        glm::mat4 viewMatrix = GetViewMatrix(*camera);
        float aspectRatio = GetWindowAspectRatio();
        glm::mat4 projMatrix = glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 100.0f);
        float view[16]; float proj[16];
        memcpy(view, glm::value_ptr(viewMatrix), sizeof(float) * 16);
        memcpy(proj, glm::value_ptr(projMatrix), sizeof(float) * 16);
        bgfx::setViewTransform(0, view, proj);

        // Bind geometry and instance data
        bgfx::setVertexBuffer(0, firstModel->mesh.vb);
        bgfx::setIndexBuffer(firstModel->mesh.ib);
        bgfx::setInstanceDataBuffer(&idb);

        // State and submit
        bgfx::setState(BGFX_STATE_DEFAULT);
        bgfx::submit(0, bgfxMeshProgram);
    }
}

void C6GE::RenderBGFXCube() {
    // Clear debug text area first to prevent duplicates on resize
    bgfx::dbgTextClear();
    
    // Use default view for metaballs
    bgfx::dbgTextPrintf(0, 1, 0x0f, "Rendering System Active");
    bgfx::dbgTextPrintf(0, 2, 0x0f, "Using default view matrix");
    
    // Basic status info
    bgfx::dbgTextPrintf(0, 4, 0x0f, "BGFX Status:");
    bgfx::dbgTextPrintf(0, 5, 0x0f, "VB: %s, IB: %s, Shader: %s", 
        bgfx::isValid(bgfxVertexBuffer) ? "OK" : "NO",
        bgfx::isValid(bgfxIndexBuffer) ? "OK" : "NO",
        bgfx::isValid(bgfxSimpleProgram) ? "OK" : "NO");
    bgfx::dbgTextPrintf(0, 6, 0x0f, "Renderer: %s", bgfx::getRendererName(bgfx::getRendererType()));
    
    // Display transform info for all objects with TransformComponent
    std::vector<std::string> transformObjects = GetAllObjectsWithComponent<TransformComponent>();
    int debugLine = 8;
    for (const auto& objName : transformObjects) {
        auto* transform = GetComponent<TransformComponent>(objName);
        if (transform && debugLine < 20) { // Limit to prevent overflow
            bgfx::dbgTextPrintf(0, debugLine++, 0x0f, "%s Transform:", objName.c_str());
            bgfx::dbgTextPrintf(0, debugLine++, 0x0f, "Pos: %.2f, %.2f, %.2f", 
                transform->Position.x, transform->Position.y, transform->Position.z);
            bgfx::dbgTextPrintf(0, debugLine++, 0x0f, "Rot: %.2f, %.2f, %.2f", 
                transform->Rotation.x, transform->Rotation.y, transform->Rotation.z);
            bgfx::dbgTextPrintf(0, debugLine++, 0x0f, "Scale: %.2f, %.2f, %.2f", 
                transform->Scale.x, transform->Scale.y, transform->Scale.z);
            debugLine++; // Add space between objects
        }
    }
    
    // Display controls info
    bgfx::dbgTextPrintf(0, debugLine++, 0x0f, "Camera Controls:");
    bgfx::dbgTextPrintf(0, debugLine++, 0x0f, "WASD: Move, Mouse: Look");
    bgfx::dbgTextPrintf(0, debugLine++, 0x0f, "Table Controls:");
    bgfx::dbgTextPrintf(0, debugLine++, 0x0f, "IJKL: Move, Arrow Keys: Rotate");
    bgfx::dbgTextPrintf(0, debugLine++, 0x0f, "UO: Forward/Back, +/-: Scale, R: Reset");
    
    // Calculate and display FPS
    static int frameCount = 0;
    static float lastTime = 0.0f;
    frameCount++;
    
    float currentTime = static_cast<float>(glfwGetTime());
    if (currentTime - lastTime >= 1.0f) {
        float fps = static_cast<float>(frameCount) / (currentTime - lastTime);
        bgfx::dbgTextPrintf(0, 7, 0x0f, "FPS: %.1f", fps);
        frameCount = 0;
        lastTime = currentTime;
    }
    
    // Render metaballs only (DISABLED)
    // C6GE::RenderMetaballs();
    
    // Render raymarching
    C6GE::RenderRaymarch();
    
    // Display metaball info (DISABLED)
    bgfx::dbgTextPrintf(0, 9, 0x0b, "METABALLS: DISABLED");
    bgfx::dbgTextPrintf(0, 10, 0x0b, "Metaball shader: %s", bgfx::isValid(bgfxMetaballProgram) ? "OK" : "NO");
    bgfx::dbgTextPrintf(0, 11, 0x0b, "Grid size: %dx%dx%d", kMaxDims, kMaxDims, kMaxDims);
    
    // Display raymarch info
    bgfx::dbgTextPrintf(0, 13, 0x0e, "RENDERING RAYMARCH:");
    bgfx::dbgTextPrintf(0, 14, 0x0e, "Raymarch shader: %s", bgfx::isValid(bgfxRaymarchProgram) ? "OK" : "NO");
    
    // Display mesh info
    bgfx::dbgTextPrintf(0, 16, 0x0d, "RENDERING MESH:");
    bgfx::dbgTextPrintf(0, 17, 0x0d, "Mesh shader: %s", bgfx::isValid(bgfxMeshProgram) ? "OK" : "NO");
}

// BGFX-specific rendering functions
void C6GE::ClearBGFX(float r, float g, float b, float a) {
    // Convert float values (0.0-1.0) to RGBA8 format
    uint32_t rgba = 0;
    rgba |= (uint32_t)(r * 255.0f) << 24;  // R
    rgba |= (uint32_t)(g * 255.0f) << 16;  // G
    rgba |= (uint32_t)(b * 255.0f) << 8;   // B
    rgba |= (uint32_t)(a * 255.0f);        // A
    
    // Set the clear color for view 0
    bgfx::setViewClear(0, 
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
        rgba, 1.0f, 0);
    
    // Touch the view to ensure it gets cleared
    bgfx::touch(0);
}

void C6GE::PresentBGFX() {
    // Submit the frame - this is crucial for proper frame timing
    bgfx::frame();
    
    // Also ensure we're processing events
    GLFWwindow* window = glfwGetCurrentContext();
    if (window) {
        glfwPollEvents();
    }
}

void C6GE::UpdateBGFXViewport() {
    GLFWwindow* window = glfwGetCurrentContext();
    if (window) {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        bgfx::reset(static_cast<uint32_t>(width), static_cast<uint32_t>(height), BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));
        
        // Note: HDR framebuffer will be updated in BeginHDR() on next frame
    }
}

float C6GE::GetWindowAspectRatio() {
    GLFWwindow* window = glfwGetCurrentContext();
    if (window) {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        if (height > 0) {
            return static_cast<float>(width) / static_cast<float>(height);
        }
    }
    return 1.0f; // Default aspect ratio if window not available
}

void C6GE::WindowResizeCallback(GLFWwindow* window, int width, int height) {
    if (currentRenderer == C6GE::RendererType::BGFX) {
        bgfx::reset(static_cast<uint32_t>(width), static_cast<uint32_t>(height), BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));
        
        // Note: HDR framebuffer will be updated in BeginHDR() on next frame
    }
}

C6GE::RendererType C6GE::GetCurrentRenderer() {
    return currentRenderer;
}

void C6GE::CleanupBGFXResources() {
    if (currentRenderer == C6GE::RendererType::BGFX) {
        // Destroy BGFX resources explicitly
        if (bgfx::isValid(bgfxVertexBuffer)) {
            bgfx::destroy(bgfxVertexBuffer);
            bgfxVertexBuffer = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(bgfxIndexBuffer)) {
            bgfx::destroy(bgfxIndexBuffer);
            bgfxIndexBuffer = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(bgfxSimpleProgram)) {
            bgfx::destroy(bgfxSimpleProgram);
            bgfxSimpleProgram = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(s_vertexShader)) {
            bgfx::destroy(s_vertexShader);
            s_vertexShader = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(s_fragmentShader)) {
            bgfx::destroy(s_fragmentShader);
            s_fragmentShader = BGFX_INVALID_HANDLE;
    }
}
}