#include "Engine/Engine.h"

int main() {
	// Initialize the C6GE engine
	if (!C6GE::Init()) return -1;

	C6GE::Update();

	C6GE::Shutdown();
	return 0;
}