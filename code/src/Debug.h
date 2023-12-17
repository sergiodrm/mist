// header file for vkmmc project 
#pragma once
#include "Logger.h"

#define expand(x) (x)
#define check(expr) \
do \
{\
	if (!expand(expr)) \
	{ \
		vkmmc::Logf(vkmmc::LogLevel::Error, "Check failed: %s\n", #expr);	\
		vkmmc_debug::PrintCallstack(); \
		__debugbreak();\
		assert(false && #expr);\
	}\
} while(0)

#define vkcheck(expr) check((VkResult)expand(expr) == VK_SUCCESS)

namespace vkmmc_debug
{
	void PrintCallstack(size_t count = 0, size_t offset = 0);
}