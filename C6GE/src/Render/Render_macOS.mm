#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

extern "C" void* setupMetalLayer(void* nwh) {
    NSWindow* window = (NSWindow*)nwh;
    NSView* contentView = [window contentView];
    [contentView setWantsLayer:YES];
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    [contentView setLayer:metalLayer];
    return metalLayer;
}

#endif