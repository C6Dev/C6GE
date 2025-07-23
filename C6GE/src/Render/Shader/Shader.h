#include <string>
#include <fstream>
#include <ios>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <libgen.h>
#elif defined(__linux__)
#include <unistd.h>
#include <libgen.h>
#elif defined(_WIN32)
#include <windows.h>
#endif
#include "Logging/Log.h"


namespace C6GE {

    // Loads a shader file and returns its contents as a const char*
    const char* LoadShader(const std::string& path);
}