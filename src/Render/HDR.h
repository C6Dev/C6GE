#pragma once

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <string>

namespace C6GE {

class HDR {
public:
    HDR();
    ~HDR();

    // Initialize HDR system with screen dimensions
    bool init(uint32_t width, uint32_t height);
    
    // Resize HDR system when window size changes
    void resize(uint32_t width, uint32_t height);
    
    // Shutdown and cleanup
    void shutdown();

    // Get HDR frame buffer for rendering
    bgfx::FrameBufferHandle getHDRFrameBuffer() const { return m_fbh; }
    
    // Get HDR texture for post-processing
    bgfx::TextureHandle getHDRTexture() const { return m_fbtextures[0]; }

    // Render HDR content to screen with tonemapping
    void renderHDRToScreen(bgfx::TextureHandle hdrTexture);

    // HDR parameters
    float getMiddleGray() const { return m_middleGray; }
    float getWhitePoint() const { return m_white; }
    float getThreshold() const { return m_threshold; }
    
    void setMiddleGray(float value) { m_middleGray = value; }
    void setWhitePoint(float value) { m_white = value; }
    void setThreshold(float value) { m_threshold = value; }

    // Get current luminance average (for debugging)
    float getLuminanceAverage() const { return m_luminanceAverage; }

private:
    // Initialize shaders
    bool initShaders();
    
    // Initialize frame buffers
    bool initFrameBuffers(uint32_t width, uint32_t height);
    
    // Cleanup frame buffers
    void cleanupFrameBuffers();
    
    // Helper functions for luminance calculation
    void setOffsets2x2Lum(bgfx::UniformHandle handle, uint32_t width, uint32_t height);
    void setOffsets4x4Lum(bgfx::UniformHandle handle, uint32_t width, uint32_t height);
    
    // Screen space quad for post-processing
    void screenSpaceQuad(bool originBottomLeft = false, float width = 1.0f, float height = 1.0f);

    // Shaders
    bgfx::ProgramHandle m_skyProgram;
    bgfx::ProgramHandle m_lumProgram;
    bgfx::ProgramHandle m_lumAvgProgram;
    bgfx::ProgramHandle m_blurProgram;
    bgfx::ProgramHandle m_brightProgram;
    bgfx::ProgramHandle m_tonemapProgram;

    // Uniforms
    bgfx::UniformHandle s_texColor;
    bgfx::UniformHandle s_texLum;
    bgfx::UniformHandle s_texBlur;
    bgfx::UniformHandle u_tonemap;
    bgfx::UniformHandle u_offset;

    // Frame buffers
    bgfx::TextureHandle m_fbtextures[2];  // Color and depth
    bgfx::FrameBufferHandle m_fbh;        // Main HDR frame buffer
    bgfx::FrameBufferHandle m_lum[5];     // Luminance chain
    bgfx::FrameBufferHandle m_bright;     // Bright pass
    bgfx::FrameBufferHandle m_blur;       // Blur pass

    // Readback texture for luminance
    bgfx::TextureHandle m_rb;
    uint32_t m_lumBgra8;

    // Dimensions
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_oldWidth;
    uint32_t m_oldHeight;

    // HDR parameters
    float m_middleGray;
    float m_white;
    float m_threshold;
    float m_luminanceAverage;

    // State
    bool m_initialized;
    const bgfx::Caps* m_caps;
};

} // namespace C6GE
