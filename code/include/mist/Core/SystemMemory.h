// header file for Mist project 
#pragma once

void* operator new(size_t size, const char* file, int line);
void operator delete(void* p);

namespace Mist
{
	void* Malloc(size_t size, const char* file, int line);
	void Free(void* p);

}

#define mistnew ::new(__FILE__, __LINE__)
#define mistdelete ::delete

