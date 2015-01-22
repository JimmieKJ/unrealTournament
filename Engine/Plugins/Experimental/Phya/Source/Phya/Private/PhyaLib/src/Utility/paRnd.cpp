//
// paRnd.cpp
//
#include "PhyaPluginPrivatePCH.h"

#if 0
#include "Utility/paRnd.hpp"
#include "System/paRand.hpp"


paFloat
paRnd(paFloat start, paFloat end)
{
	return start+(end-start)* paRand() / paRAND_MAX;
}


int
paRnd(int start, int end)
{
	return start+ (int)((paFloat)(end-start+1) * (paFloat)paRand() / (paFloat)paRAND_MAX);
}

#endif // 0
