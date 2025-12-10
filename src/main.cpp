#include "d_engine.h"

int main(int argc, char* argv[]) {
    DirectEngine engine;

    engine.Init();

    engine.Run();

    engine.Cleanup();
    
    return 0;
}