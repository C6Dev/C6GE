#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#import <sqlite3.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include "../Logging/Log.h"

namespace C6GE {

bool InitBGFX_macOS(void* windowPtr) {
    Log(LogLevel::info, "Initializing BGFX on macOS...");

    if (!windowPtr) {
        Log(LogLevel::error, "No window available for BGFX.");
        return false;
    }
    
    // Cast to NSWindow and ensure it's visible
    NSWindow* window = static_cast<NSWindow*>(windowPtr);
    [window makeKeyAndOrderFront:nil];
    
    bgfx::PlatformData pd{};
    // macOS: Use NSWindow directly for BGFX
    pd.nwh = window; // NSWindow*
    pd.ndt = [window contentView]; // NSView*
    Log(LogLevel::info, "Cocoa window handle: " + std::to_string(reinterpret_cast<uintptr_t>(pd.nwh)));
    Log(LogLevel::info, "Cocoa view handle: " + std::to_string(reinterpret_cast<uintptr_t>(pd.ndt)));
    
    // Set platform data before initialization
    bgfx::setPlatformData(pd);
    
    // Get actual window size from NSWindow
    NSRect frame = [window frame];
    int windowWidth = static_cast<int>(frame.size.width);
    int windowHeight = static_cast<int>(frame.size.height);
    
    bgfx::Init init{};
    init.resolution.width  = static_cast<uint32_t>(windowWidth);
    init.resolution.height = static_cast<uint32_t>(windowHeight);
    init.resolution.reset  = BGFX_RESET_VSYNC; // Try with vsync enabled
    
    Log(LogLevel::info, "BGFX init resolution: " + std::to_string(init.resolution.width) + "x" + std::to_string(init.resolution.height));
    
    // Try different renderers
    std::vector<bgfx::RendererType::Enum> renderers = {
        bgfx::RendererType::Metal,
        bgfx::RendererType::OpenGL
    };
    
    bool initialized = false;
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
        Log(LogLevel::error, "BGFX failed to initialize any renderer.");
        return false;
    }
    
    // Set debug flags (optional)
    // bgfx::setDebug(BGFX_DEBUG_TEXT); // Comment out debug for now
    bgfx::setViewClear(0,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
        0x303030ff, 1.0f, 0);

    // Simple test frame
    bgfx::touch(0);
    bgfx::frame();

    Log(LogLevel::info, "BGFX initialized successfully on macOS.");
    return true;
}

} // namespace C6GE

#endif // __APPLE__
