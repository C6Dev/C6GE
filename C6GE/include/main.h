#pragma once

#if defined(_WIN32) || defined(_WIN64)

#  ifdef C6GE_EXPORTS
#    define C6GE_API __declspec(dllexport)
#  else
#    define C6GE_API __declspec(dllimport)
#  endif

#else

// On non-Windows platforms, fall back to GCC/Clang visibility attributes
#  if __GNUC__ >= 4
#    define C6GE_API __attribute__((visibility("default")))
#  else
#    define C6GE_API
#  endif

#endif