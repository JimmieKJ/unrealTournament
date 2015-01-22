//
// paRnd.hpp
//


#ifndef __paRnd_hpp
#define __paRnd_hpp

#include "System/paFloat.h"
#include "System/paRand.hpp"

#include "stdio.h"


inline int
paRnd(const int start, const int end)
{
	return start + (int)((paFloat)(end+1 - start) * pa1divRandMaxPlus1   * (paFloat)paRand());
}

inline paFloat
paRnd(const paFloat start, const paFloat end)
{
	paFloat r;
	r = start + (end - start) * pa1divRandMaxPlus1  * (paFloat)paRand();
	return r;
}

inline paFloat
paRnd(const paFloat end)
{
	paFloat r;
	r = end * pa1divRandMaxPlus1  * (paFloat)paRand();
	return r;
}



#endif

