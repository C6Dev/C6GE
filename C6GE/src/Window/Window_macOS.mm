#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#import <sqlite3.h>
#include "Window.h"
#include "../Logging/Log.h"

namespace C6GE {

// macOS-specific window management
static NSWindow* g_window = nullptr;
static NSApplication* g_app = nullptr;

// Global variables (extern declarations)
extern float fov;
extern float nearPlane;
extern float farPlane;
extern glm::mat4 projectionMatrix;

bool CreateWindow_macOS(int width, int height, const char* title) {
    Log(LogLevel::info, "Creating Cocoa window for macOS BGFX compatibility");
    
    // Initialize NSApplication if not already done
    if (!g_app) {
        g_app = [NSApplication sharedApplication];
        [g_app setActivationPolicy:NSApplicationActivationPolicyRegular];
    }
    
    // Create window
    NSRect frame = NSMakeRect(100, 100, width, height);
    g_window = [[NSWindow alloc] initWithContentRect:frame
                                        styleMask:NSWindowStyleMaskTitled | 
                                                  NSWindowStyleMaskClosable | 
                                                  NSWindowStyleMaskMiniaturizable | 
                                                  NSWindowStyleMaskResizable
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
    
    if (!g_window) {
        Log(LogLevel::error, "Failed to create Cocoa window");
        return false;
    }
    
    [g_window setTitle:[NSString stringWithUTF8String:title]];
    [g_window makeKeyAndOrderFront:nil];
    [g_window center];
    
    Log(LogLevel::info, "Cocoa window created successfully");
    return true;
}

void UpdateWindow_macOS() {
    // Process Cocoa events
    NSEvent* event;
    while ((event = [g_app nextEventMatchingMask:NSEventMaskAny
                                    untilDate:[NSDate distantPast]
                                       inMode:NSDefaultRunLoopMode
                                      dequeue:YES])) {
        [g_app sendEvent:event];
    }
    
    // Update projection matrix based on window size
    if (g_window) {
        NSRect frame = [g_window frame];
        int framebufferWidth = static_cast<int>(frame.size.width);
        int framebufferHeight = static_cast<int>(frame.size.height);
        
        float aspect = static_cast<float>(framebufferWidth) / framebufferHeight;
        projectionMatrix = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
    }
}

bool IsWindowOpen_macOS() {
    return g_window && ![g_window isVisible];
}

void DestroyWindow_macOS() {
    if (g_window) {
        [g_window release];
        g_window = nullptr;
    }
    if (g_app) {
        [g_app terminate:nil];
        g_app = nullptr;
    }
}

void* GetWindow_macOS() {
    return g_window;
}

} // namespace C6GE

#endif // __APPLE__
