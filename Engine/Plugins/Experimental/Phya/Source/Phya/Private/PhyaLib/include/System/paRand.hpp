//
// paRand.hpp
//
// Wrapper for general purpose random function.
//


#ifndef __paRand_hpp
#define __paRand_hpp


#include "System/paFloat.h"

const paFloat paRandMax = 4294967296;
const paFloat pa1divRandMaxPlus1 = 1/(paRandMax+1);


extern unsigned int low;
extern unsigned int high;

inline unsigned int paRand() //gamerand()
{
    high = (high << 16) + (high >> 16);
    high += low;
    low += high;
	

    return high;
};





//#include <stdlib.h>
//
//#define paRAND_MAX    RAND_MAX
//
//extern int _paRandSeed;
//
//
//inline int paRand()
//{
// int r = rand();
// paFloat f = r / (paFloat) paRAND_MAX;
// return rand();
//}
//


//
//// faster rand, not quite so random but ok for audio.
////
////	PUBLIC CS_PURE int csoundRand31(int seedVal)  // By Robin Whittle.
////	{
////	long long tmp1;
////	long tmp2;
//
//	//long long tmp1;
//	//long tmp2;
//
//	///* x = (742938285 * x) % 0x7FFFFFFF */
//	//tmp1 = (long long) ((long) _paRandSeed * (long long) 742938285);
//	//tmp2 = (long) tmp1 & (long) 0x7FFFFFFF;
//	//tmp2 += (long) (tmp1 >> 31);
//	//if ((long) tmp2 < (long) 0)
//	//tmp2 = (tmp2 + (long) 1) & (long) 0x7FFFFFFF;
//	//_paRandSeed = tmp2;
//	//return (int) tmp2;
//}



#endif
