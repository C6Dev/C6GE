# C6GE - Century6 Game Engine

![C6GE Logo](https://github.com/user-attachments/assets/fd87a175-37c9-41e5-aaf0-0c77045000ec)

**C6GE** is a game engine built on top of the Diligent Engine and aimed at delivering cross-platform rendering capabilities with support for modern graphics APIs.  
⚠️ *Early development stage — expect major changes and evolving architecture.*

---

## 🧪 Unit Tests

| Platform | Status |
|----------|--------|
| Linux    | ![Linux Unit Test](https://github.com/C6Dev/C6GE/actions/workflows/LinuxUnitTest.yml/badge.svg) |
| Windows  | ![Windows Unit Test](https://github.com/C6Dev/C6GE/actions/workflows/WindowsUnitTest.yml/badge.svg) |
| macOS    | ![macOS Unit Test](https://github.com/C6Dev/C6GE/actions/workflows/MacOSUnitTest.yml/badge.svg) |

---

## Supported Rendering Backends

The engine currently supports the following rendering APIs:

- **DirectX 11** – Full support on Windows  
- **DirectX 12** – Full support on Windows  
- **Vulkan** – Cross-platform support (Windows, Linux, macOS)  
- **Metal** – Experimental support (macOS and Apple Silicon)  

> _Note: The Metal backend is considered experimental at this time. Use for testing and exploration; production readiness may require further validation._

---

## Supported Platforms

- **Windows**  
  - ✅ DirectX 11  
  - ✅ DirectX 12  
  - ✅ Vulkan  
- **Linux**  
  - ✅ Vulkan  
- **macOS (including Apple Silicon)**  
  - ✅ Vulkan  
  - ⚠️ Metal (experimental)  

---

## Game Engine Editor

The engine includes an editor built with **ImGui** (docking branch) to support advanced window/viewport integration required for engine workflows.

---

## Building the Engine

Build instructions coming soon.  
For now, you can refer to the `CMakeLists.txt` and `samples/` folder to get started.

---

## Contributing

This project is in active development and helps are welcome. If you contribute, please be aware of the rapidly evolving codebase.

---

## License

This project is licensed under the Apache 2.0 License.  
