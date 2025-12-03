#pragma once

#if defined(_WIN32) || defined(_WIN64)

#  ifdef DirectEngine_EXPORTS
#    define DirectEngine_API __declspec(dllexport)
#  else
#    define DirectEngine_API __declspec(dllimport)
#  endif

#else

// On non-Windows platforms, fall back to GCC/Clang visibility attributes
#  if __GNUC__ >= 4
#    define DirectEngine_API __attribute__((visibility("default")))
#  else
#    define DirectEngine_API
#  endif

#endif