#pragma once

#include <bgfx/bgfx.h>
#include <bx/filepath.h>
#include <bx/string.h>
#include <string>

// Forward declaration
struct Mesh;

class MeshLoader {
public:
    MeshLoader();
    ~MeshLoader();

    // Load a mesh file, automatically converting if needed
    Mesh* loadMesh(const char* filePath, bool ramcopy = false);
    
    // Load a mesh file with explicit conversion
    Mesh* loadMeshWithConversion(const char* filePath, bool ramcopy = false);
    
    // Pre-convert mesh files to .bin format (for engine initialization)
    bool preConvertMesh(const char* filePath);
    
    // Convert a mesh file to .bin format
    bool convertToBin(const bx::FilePath& inputPath, const bx::FilePath& outputPath);
    
    // Check if a file exists
    bool fileExists(const char* filePath);
    
    // Get the last write time of a file
    uint64_t getLastWriteTime(const char* filePath);

private:
    // Helper to check if .bin file is newer than source
    bool isBinFileNewer(const bx::FilePath& sourcePath, const bx::FilePath& binPath);
    
    // Helper to get file extension
    bx::StringView getFileExtension(const char* filePath);
    
    // Helper to change file extension
    std::string changeFileExtension(const char* filePath, const char* newExt);
};
