#include "HDR.h"
#include <bgfx/bgfx.h>
#include <bgfx_utils.h>
#include <bx/math.h>
#include <iostream>

namespace C6GE {

// Vertex layout for screen space quad
struct PosColorTexCoord0Vertex
{
    float m_x;
    float m_y;
    float m_z;
    uint32_t m_rgba;
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

HDR::HDR()
    : m_initialized(false)
    , m_width(0)
    , m_height(0)
    , m_oldWidth(0)
    , m_oldHeight(0)
    , m_middleGray(0.18f)
    , m_white(1.1f)
    , m_threshold(1.5f)
    , m_luminanceAverage(0.0f)
    , m_lumBgra8(0)
    , m_caps(nullptr)
{
    // Initialize handles to invalid
    m_fbh.idx = bgfx::kInvalidHandle;
    m_rb.idx = bgfx::kInvalidHandle;
    
    for (int i = 0; i < 2; ++i) {
        m_fbtextures[i].idx = bgfx::kInvalidHandle;
    }
    
    for (int i = 0; i < 5; ++i) {
        m_lum[i].idx = bgfx::kInvalidHandle;
    }
    
    m_bright.idx = bgfx::kInvalidHandle;
    m_blur.idx = bgfx::kInvalidHandle;
}

HDR::~HDR()
{
    shutdown();
}

bool HDR::init(uint32_t width, uint32_t height)
{
    if (m_initialized) {
        std::cout << "HDR system already initialized" << std::endl;
        return true;
    }

    m_width = width;
    m_height = height;
    m_caps = bgfx::getCaps();

    // Initialize vertex layout
    PosColorTexCoord0Vertex::init();

    // Initialize shaders
    if (!initShaders()) {
        std::cout << "Failed to initialize HDR shaders" << std::endl;
        return false;
    }

    // Initialize frame buffers
    if (!initFrameBuffers(width, height)) {
        std::cout << "Failed to initialize HDR frame buffers" << std::endl;
        return false;
    }

    m_initialized = true;
    std::cout << "HDR system initialized successfully" << std::endl;
    return true;
}

bool HDR::initShaders()
{
    // For now, we'll use a simplified approach
    // The HDR effect will be applied through the existing rendering pipeline
    std::cout << "HDR shaders initialized (simplified)" << std::endl;
    
    // Create uniforms
    s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    s_texLum = bgfx::createUniform("s_texLum", bgfx::UniformType::Sampler);
    s_texBlur = bgfx::createUniform("s_texBlur", bgfx::UniformType::Sampler);
    u_tonemap = bgfx::createUniform("u_tonemap", bgfx::UniformType::Vec4);
    u_offset = bgfx::createUniform("u_offset", bgfx::UniformType::Vec4, 16);

    return true;
}

bool HDR::initFrameBuffers(uint32_t width, uint32_t height)
{
    uint32_t msaa = 0; // Simplified for now

    // Create HDR color texture
    m_fbtextures[0] = bgfx::createTexture2D(
        uint16_t(width),
        uint16_t(height),
        false,
        1,
        bgfx::TextureFormat::BGRA8,
        (uint64_t(msaa + 1) << BGFX_TEXTURE_RT_MSAA_SHIFT) | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );

    // Create depth texture
    const uint64_t textureFlags = BGFX_TEXTURE_RT_WRITE_ONLY | (uint64_t(msaa + 1) << BGFX_TEXTURE_RT_MSAA_SHIFT);
    
    bgfx::TextureFormat::Enum depthFormat =
        bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D16, textureFlags) ? bgfx::TextureFormat::D16
        : bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D24S8, textureFlags) ? bgfx::TextureFormat::D24S8
        : bgfx::TextureFormat::D32;

    m_fbtextures[1] = bgfx::createTexture2D(
        uint16_t(width),
        uint16_t(height),
        false,
        1,
        depthFormat,
        textureFlags
    );

    // Create main HDR frame buffer
    m_fbh = bgfx::createFrameBuffer(2, m_fbtextures, true);

    // Create luminance chain
    m_lum[0] = bgfx::createFrameBuffer(128, 128, bgfx::TextureFormat::BGRA8);
    m_lum[1] = bgfx::createFrameBuffer(64, 64, bgfx::TextureFormat::BGRA8);
    m_lum[2] = bgfx::createFrameBuffer(16, 16, bgfx::TextureFormat::BGRA8);
    m_lum[3] = bgfx::createFrameBuffer(4, 4, bgfx::TextureFormat::BGRA8);
    m_lum[4] = bgfx::createFrameBuffer(1, 1, bgfx::TextureFormat::BGRA8);

    // Create bright pass and blur frame buffers
    m_bright = bgfx::createFrameBuffer(bgfx::BackbufferRatio::Half, bgfx::TextureFormat::BGRA8);
    m_blur = bgfx::createFrameBuffer(bgfx::BackbufferRatio::Eighth, bgfx::TextureFormat::BGRA8);

    // Create readback texture for luminance
    if ((BGFX_CAPS_TEXTURE_BLIT | BGFX_CAPS_TEXTURE_READ_BACK) == 
        (bgfx::getCaps()->supported & (BGFX_CAPS_TEXTURE_BLIT | BGFX_CAPS_TEXTURE_READ_BACK))) {
        m_rb = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::BGRA8, 
                                   BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
    }

    return true;
}

void HDR::cleanupFrameBuffers()
{
    if (bgfx::isValid(m_fbh)) {
        bgfx::destroy(m_fbh);
        m_fbh.idx = bgfx::kInvalidHandle;
    }

    for (int i = 0; i < 2; ++i) {
        if (bgfx::isValid(m_fbtextures[i])) {
            bgfx::destroy(m_fbtextures[i]);
            m_fbtextures[i].idx = bgfx::kInvalidHandle;
        }
    }

    for (int i = 0; i < 5; ++i) {
        if (bgfx::isValid(m_lum[i])) {
            bgfx::destroy(m_lum[i]);
            m_lum[i].idx = bgfx::kInvalidHandle;
        }
    }

    if (bgfx::isValid(m_bright)) {
        bgfx::destroy(m_bright);
        m_bright.idx = bgfx::kInvalidHandle;
    }

    if (bgfx::isValid(m_blur)) {
        bgfx::destroy(m_blur);
        m_blur.idx = bgfx::kInvalidHandle;
    }

    if (bgfx::isValid(m_rb)) {
        bgfx::destroy(m_rb);
        m_rb.idx = bgfx::kInvalidHandle;
    }
}

void HDR::resize(uint32_t width, uint32_t height)
{
    if (!m_initialized || (m_width == width && m_height == height)) {
        return;
    }

    m_oldWidth = m_width;
    m_oldHeight = m_height;
    m_width = width;
    m_height = height;

    cleanupFrameBuffers();
    initFrameBuffers(width, height);
}

void HDR::shutdown()
{
    if (!m_initialized) {
        return;
    }

    // Cleanup shaders (none for now)

    // Cleanup uniforms
    if (bgfx::isValid(s_texColor)) {
        bgfx::destroy(s_texColor);
    }
    if (bgfx::isValid(s_texLum)) {
        bgfx::destroy(s_texLum);
    }
    if (bgfx::isValid(s_texBlur)) {
        bgfx::destroy(s_texBlur);
    }
    if (bgfx::isValid(u_tonemap)) {
        bgfx::destroy(u_tonemap);
    }
    if (bgfx::isValid(u_offset)) {
        bgfx::destroy(u_offset);
    }

    cleanupFrameBuffers();

    m_initialized = false;
    std::cout << "HDR system shutdown" << std::endl;
}

void HDR::renderHDRToScreen(bgfx::TextureHandle hdrTexture)
{
    if (!m_initialized) {
        return;
    }

    // For now, we'll just copy the HDR texture to screen
    // In a full implementation, this would apply proper tonemapping
    bgfx::ViewId hdrView = 1;
    
    bgfx::setViewName(hdrView, "HDR to Screen");
    bgfx::setViewRect(hdrView, 0, 0, bgfx::BackbufferRatio::Equal);
    
    // Set up projection matrix for screen space quad
    float proj[16];
    bx::mtxOrtho(proj, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 0.0f, m_caps->homogeneousDepth);
    bgfx::setViewTransform(hdrView, NULL, proj);
    
    // Set HDR texture
    bgfx::setTexture(0, s_texColor, hdrTexture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    
    // For now, we'll just use a simple pass-through
    // In a full implementation, you'd submit to a tonemap program
    screenSpaceQuad(m_caps->originBottomLeft);
    
    // Note: We're not submitting to a program for now, just setting up the view
    // This will show the HDR texture directly without tonemapping
}

void HDR::setOffsets2x2Lum(bgfx::UniformHandle handle, uint32_t width, uint32_t height)
{
    float offsets[16][4];

    float du = 1.0f / width;
    float dv = 1.0f / height;

    uint16_t num = 0;
    for (uint32_t yy = 0; yy < 3; ++yy) {
        for (uint32_t xx = 0; xx < 3; ++xx) {
            offsets[num][0] = xx * du;
            offsets[num][1] = yy * dv;
            ++num;
        }
    }

    bgfx::setUniform(handle, offsets, num);
}

void HDR::setOffsets4x4Lum(bgfx::UniformHandle handle, uint32_t width, uint32_t height)
{
    float offsets[16][4];

    float du = 1.0f / width;
    float dv = 1.0f / height;

    uint16_t num = 0;
    for (uint32_t yy = 0; yy < 4; ++yy) {
        for (uint32_t xx = 0; xx < 4; ++xx) {
            offsets[num][0] = (xx - 1.0f) * du;
            offsets[num][1] = (yy - 1.0f) * dv;
            ++num;
        }
    }

    bgfx::setUniform(handle, offsets, num);
}

void HDR::screenSpaceQuad(bool originBottomLeft, float width, float height)
{
    if (3 == bgfx::getAvailTransientVertexBuffer(3, PosColorTexCoord0Vertex::ms_layout)) {
        bgfx::TransientVertexBuffer vb;
        bgfx::allocTransientVertexBuffer(&vb, 3, PosColorTexCoord0Vertex::ms_layout);
        PosColorTexCoord0Vertex* vertex = (PosColorTexCoord0Vertex*)vb.data;

        const float zz = 0.0f;

        const float minx = -width;
        const float maxx = width;
        const float miny = 0.0f;
        const float maxy = height * 2.0f;

        const float minu = -1.0f;
        const float maxu = 1.0f;

        float minv = 0.0f;
        float maxv = 2.0f;

        if (originBottomLeft) {
            float temp = minv;
            minv = maxv;
            maxv = temp;

            minv -= 1.0f;
            maxv -= 1.0f;
        }

        vertex[0].m_x = minx;
        vertex[0].m_y = miny;
        vertex[0].m_z = zz;
        vertex[0].m_rgba = 0xffffffff;
        vertex[0].m_u = minu;
        vertex[0].m_v = minv;

        vertex[1].m_x = maxx;
        vertex[1].m_y = miny;
        vertex[1].m_z = zz;
        vertex[1].m_rgba = 0xffffffff;
        vertex[1].m_u = maxu;
        vertex[1].m_v = minv;

        vertex[2].m_x = maxx;
        vertex[2].m_y = maxy;
        vertex[2].m_z = zz;
        vertex[2].m_rgba = 0xffffffff;
        vertex[2].m_u = maxu;
        vertex[2].m_v = maxv;

        bgfx::setVertexBuffer(0, &vb);
    }
}

} // namespace C6GE
