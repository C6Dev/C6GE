#include "Engine/Engine.h"

// Stub for entry framework
extern "C" int _main_(int _argc, char** _argv) {
    return 0; // We don't use the entry framework's main
}

int main() {
    C6GE::EngineRun();
    return 0;
}