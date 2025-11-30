#pragma once

#ifdef C6GE_EXPORTS
#define C6GE_API __declspec(dllexport)
#else
#define C6GE_API __declspec(dllimport)
#endif

C6GE_API void EngineInit();
C6GE_API void EngineShutdown();