//
// paMemory.hpp
//


#if !defined(__paMemory_hpp)
#define __paMemory_hpp


#include <memory.h>
#include <stdlib.h>

#include "System/paConfig.h"
//#include "paAPI.h"


PHYA_API void*	paMalloc( long size );
PHYA_API void*	paCalloc( long num, int size );
PHYA_API void	paFree( void *memblock );
PHYA_API void*	paMemcpy( void*, const void *, long size );

#define paFloatCalloc(n) (paFloat*)paCalloc(n, sizeof(paFloat));




#endif
