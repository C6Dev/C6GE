#pragma once

#ifdef C6GE_EXPORTS
#if defined(_WIN32) || defined(_WIN64)
#define C6GE_API __declspec(dllexport)
#else
#define C6GE_API
#endif
#else
#define C6GE_API __declspec(dllimport)
#endif

C6GE_API void EngineInit();
C6GE_API void EngineShutdown();