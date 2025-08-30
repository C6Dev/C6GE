#include "Render.h"
#include <iostream>

class Window;

namespace C6GE {
    void Render::RenderFrame() {
        bgfx::renderFrame();
    }

    void Render::SetPlatformData(C6GE::Window& window, bgfx::PlatformData& pd) {
        #if defined(_WIN32) || defined(_WIN64)
            pd.nwh = window.GetWin32Window();
        #elif defined(__APPLE__)
            pd.nwh = window.GetCocoaWindow();
        #elif defined(__linux__)
            pd.nwh = window.GetX11Window();
            pd.ndt = window.GetX11Display();
        #endif
    }

    bool Render::Init(C6GE::Window& window, bgfx::PlatformData& pd, bgfx::CallbackI* callback) {
        bgfx::Init init;
        
        // Don't set any debug flags to minimize output
        
        #if BX_PLATFORM_WINDOWS
            // Try DX12 first, fall back to DX11 if it fails
            init.type = bgfx::RendererType::Direct3D12;
            init.resolution.width = window.GetFramebufferWidth();
            init.resolution.height = window.GetFramebufferHeight();
            init.resolution.reset = BGFX_RESET_VSYNC;
            init.platformData = pd;
            if (callback) {
                init.callback = callback;
            }
            
            if (!bgfx::init(init)) {
                std::cout << "DX12 failed, trying DX11..." << std::endl;
                init.type = bgfx::RendererType::Direct3D11;
                if (!bgfx::init(init)) {
                    std::cout << "Failed to initialize bgfx with DX11" << std::endl;
                    return false;
                }
            }
        #else
            init.type = bgfx::RendererType::Count; // Auto-detect renderer on other platforms
            init.resolution.width = window.GetFramebufferWidth();
            init.resolution.height = window.GetFramebufferHeight();
            init.resolution.reset = BGFX_RESET_VSYNC;
            init.platformData = pd;
            if (callback) {
                init.callback = callback;
            }
            
            if (!bgfx::init(init)) { 
                std::cout << "Failed to initialize bgfx" << std::endl; 
                return false; 
            }
        #endif
        return true;
    }

    void Render::UpdateWindowSize(C6GE::Window& window){
        bgfx::reset(window.GetFramebufferWidth(), window.GetFramebufferHeight(), BGFX_RESET_VSYNC);
    }
}