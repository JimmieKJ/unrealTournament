//
// paMemory.cpp
//
#include "PhyaPluginPrivatePCH.h"


#include "System/paMemory.hpp"


void*
paMalloc( long size )
{
	return malloc(size);
}



void*
paCalloc( long num, int size )
{
	return calloc( num, size );
}



void paFree( void *memblock)
{
	free(memblock);
}


void* paMemcpy( void* to, const void *from, long size )
{
	return memcpy( to, from, (size_t)size );
}

