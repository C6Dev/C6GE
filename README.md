# C6GE - Century6 Game Engine
<img width="500" height="500" alt="IMG_0793" src="https://github.com/user-attachments/assets/fd87a175-37c9-41e5-aaf0-0c77045000ec" />

C6GE is a game engine built on top of the Diligent Engine, providing cross-platform rendering capabilities with support for modern graphics APIs.

## ⚠️ Early Development Warning

**This engine is in very early development stages.** The codebase has already been completely rewritten at least 3 times, and you should expect significant changes - for better or worse. The goal is to create a great game engine, but the architecture and features are still evolving rapidly.
This project is in early development. It is a learning project, and the code quality is evolving. Help is welcome if explained well and approved.

## Supported Rendering Engines

C6GE supports the following rendering backends through Diligent Engine:

- **DirectX 11** - Full support on Windows
- **DirectX 12** - Full support on Windows
- **Vulkan** - Cross-platform support (Windows, Linux, macOS)

### Metal Rendering Support

Metal rendering is technically supported but requires **Diligent Engine Pro**. To use Metal:

- You must contact the Diligent Engine team directly to obtain Diligent Engine Pro
- **We are not responsible for any issues or agreements related to Diligent Engine Pro**
- A free Metal renderer is planned for the future through a fork of Diligent Engine

## Supported Platforms

### Windows
- ✅ DirectX 11
- ✅ DirectX 12
- ✅ Vulkan

### Linux
- ✅ Vulkan

### macOS
- ✅ Vulkan (with interpretation layer)
- ⚠️ Metal (requires Diligent Engine Pro - see above)

## Game Engine Editor

The engine uses **ImGui** with the **docking branch** to provide an editor interface. The docking branch enables advanced window management features necessary for a professional game engine editor.

## Building the Engine

(Build instructions to be added)

## Contributing

As this is an early-stage project, contributions and feedback are welcome, but please be aware of the rapid changes in the codebase.

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## Building the Engine
To build C6GE, you will need to use CMake. Here are the commands for different build options:

### Build just C6GE
```bash
mkdir build
cd build
cmake ..
make C6GE
```

### Build both C6GE and Runtime
```bash
mkdir build
cd build
cmake ..
make C6GE Runtime
```

### Build Diligent Samples
```bash
mkdir build
cd build
cmake ..
make DiligentSamples
```
```
You can find the samples in the `samples` directory. The renderer is based on the Sample Shadows tutorial.
