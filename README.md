# C6GE

## Early Development Warning
This project is in early development. It is a learning project, and the code quality is evolving. Help is welcome if explained well and approved.

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