#pragma once

#include <bgfx/bgfx.h>

// Vertex layout for mesh rendering
struct PosNormalTexCoordVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    
    static void init() {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }
    
    static bgfx::VertexLayout ms_layout;
};
