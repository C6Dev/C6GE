#include <string>
#include <fstream>
#include <ios>
#include <mach-o/dyld.h>
#include <libgen.h>
#include "Logging/Log.h"


namespace C6GE {

    // Loads a shader file and returns its contents as a const char*
    const char* LoadShader(const std::string& path);
}