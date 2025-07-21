#include "Engine/Engine.h"
#include "Logging/Log.h"


int main() {
	if (!C6GE::Init()) {
		C6GE::Log(C6GE::LogLevel::critical, "Failed to initialize C6GE engine.");
		return -1;
	}
	C6GE::Update();
	C6GE::Shutdown();
	return 0;
}