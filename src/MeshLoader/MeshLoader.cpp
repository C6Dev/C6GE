#include "MeshLoader.h"
#include <bgfx_utils.h>
#include <bx/file.h>
#include <bx/string.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <sys/stat.h> // For file timestamp checking

MeshLoader::MeshLoader() {
    // Constructor - could initialize any resources if needed
}

MeshLoader::~MeshLoader() {
    // Destructor - cleanup if needed
}

Mesh* MeshLoader::loadMesh(const char* filePath, bool ramcopy) {
    bx::FilePath path(filePath);
    bx::StringView ext = path.getExt();
    
    // If it's already a .bin file, load directly
    if (0 == bx::strCmpI(ext, ".bin")) {
        return meshLoad(filePath, ramcopy);
    }
    
    // If it's an OBJ or glTF, convert it first
    if (0 == bx::strCmpI(ext, ".obj") || 
        0 == bx::strCmpI(ext, ".gltf") || 
        0 == bx::strCmpI(ext, ".glb")) {
        
        return loadMeshWithConversion(filePath, ramcopy);
    }
    
    // Unsupported format
    std::cout << "Unsupported mesh format: " << ext.getPtr() << std::endl;
    return nullptr;
}

Mesh* MeshLoader::loadMeshWithConversion(const char* filePath, bool ramcopy) {
    bx::FilePath inputPath(filePath);
    
    // Generate output path for .bin file
    std::string outputPathStr = changeFileExtension(filePath, ".bin");
    bx::FilePath outputPath(outputPathStr.c_str());
    
    // For now, always try to convert (we can add file existence checks later)
    std::cout << "Converting " << filePath << " to .bin format..." << std::endl;
    if (convertToBin(inputPath, outputPath)) {
        std::cout << "Conversion successful, loading: " << outputPathStr << std::endl;
        return meshLoad(outputPathStr.c_str(), ramcopy);
    } else {
        std::cout << "Conversion failed, attempting direct load..." << std::endl;
        // Fallback to direct loading if conversion fails
        return meshLoad(filePath, ramcopy);
    }
}

bool MeshLoader::preConvertMesh(const char* filePath) {
    bx::FilePath inputPath(filePath);
    bx::StringView ext = inputPath.getExt();
    
    // Only convert if it's not already a .bin file
    if (0 == bx::strCmpI(ext, ".bin")) {
        std::cout << "File is already .bin format: " << filePath << std::endl;
        return true;
    }
    
    // Check if it's a supported format for conversion
    if (0 == bx::strCmpI(ext, ".obj") || 
        0 == bx::strCmpI(ext, ".gltf") || 
        0 == bx::strCmpI(ext, ".glb")) {
        
        // Generate output path for .bin file
        std::string outputPathStr = changeFileExtension(filePath, ".bin");
        bx::FilePath outputPath(outputPathStr.c_str());
        
        // Check if .bin already exists and is newer
        if (fileExists(outputPathStr.c_str()) && isBinFileNewer(inputPath, outputPath)) {
            std::cout << "Using existing .bin file: " << outputPathStr << std::endl;
            return true;
        }
        
        std::cout << "Pre-converting " << filePath << " to .bin format..." << std::endl;
        if (convertToBin(inputPath, outputPath)) {
            std::cout << "Pre-conversion successful: " << outputPathStr << std::endl;
            return true;
        } else {
            std::cout << "Pre-conversion failed: " << filePath << std::endl;
            return false;
        }
    }
    
    std::cout << "Unsupported format for pre-conversion: " << ext.getPtr() << std::endl;
    return false;
}

bool MeshLoader::convertToBin(const bx::FilePath& inputPath, const bx::FilePath& outputPath) {
    // Build geometryc command
    // Use the geometryc executable that was built in the build directory
    const char* geometrycPath = "./bgfx.cmake-build/cmake/bgfx/geometryc"; // Correct path to built geometryc
    
    char cmd[1024];
    bx::snprintf(cmd, sizeof(cmd), 
        "%s -f \"%s\" -o \"%s\" --ccw --tangent", 
        geometrycPath,
        inputPath.getCPtr(),
        outputPath.getCPtr());
    
    std::cout << "Executing: " << cmd << std::endl;
    
    // Execute the command
    int result = system(cmd);
    return result == 0;
}

bool MeshLoader::fileExists(const char* filePath) {
    // Simple file existence check using fopen
    FILE* file = fopen(filePath, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

uint64_t MeshLoader::getLastWriteTime(const char* filePath) {
    struct stat fileStat;
    if (stat(filePath, &fileStat) == 0) {
        return (uint64_t)fileStat.st_mtime;
    }
    return 0;
}

bool MeshLoader::isBinFileNewer(const bx::FilePath& sourcePath, const bx::FilePath& binPath) {
    if (!fileExists(binPath.getCPtr())) {
        return false;
    }
    
    uint64_t sourceTime = getLastWriteTime(sourcePath.getCPtr());
    uint64_t binTime = getLastWriteTime(binPath.getCPtr());
    
    return binTime > sourceTime;
}

bx::StringView MeshLoader::getFileExtension(const char* filePath) {
    bx::FilePath path(filePath);
    return path.getExt();
}

std::string MeshLoader::changeFileExtension(const char* filePath, const char* newExt) {
    bx::FilePath path(filePath);
    
    // Get the base path (without extension)
    bx::StringView basePath = path.getPath();
    bx::StringView baseName = path.getBaseName();
    
    // Construct new path with new extension
    std::string newPath;
    newPath.reserve(1024);
    
    // Add the base path
    if (basePath.getLength() > 0) {
        newPath.append(basePath.getPtr(), basePath.getLength());
    }
    
    // Add the base name
    if (baseName.getLength() > 0) {
        newPath.append(baseName.getPtr(), baseName.getLength());
    }
    
    // Add the new extension
    newPath.append(newExt);
    
    return newPath;
}
