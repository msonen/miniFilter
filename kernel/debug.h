#pragma once
#include <fltKernel.h>


//#define DEBUG_MODE
#ifdef DEBUG_MODE
#define DEBUG(...)    DbgPrint(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#define LOG(...)    DbgPrint(__VA_ARGS__)