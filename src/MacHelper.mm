// src/MacHelper.mm
#import <Cocoa/Cocoa.h>

void* GetNSViewFromNSWindow(void* nsWindow)
{
    NSWindow* window = (__bridge NSWindow*)nsWindow;
    return (__bridge void*)[window contentView];
}