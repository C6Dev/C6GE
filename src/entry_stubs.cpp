// Entry framework stubs for bgfx_utils.cpp
#include <entry/entry.h>
#include <bx/allocator.h>
#include <bx/file.h>

namespace entry
{
    // Simple allocator implementation
    static bx::DefaultAllocator s_allocator;
    static bx::FileReader s_fileReader;

    bx::AllocatorI* getAllocator()
    {
        return &s_allocator;
    }

    bx::FileReaderI* getFileReader()
    {
        return &s_fileReader;
    }
}
