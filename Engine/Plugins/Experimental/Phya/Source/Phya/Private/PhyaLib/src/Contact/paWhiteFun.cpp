//
// paWhiteFun.cpp
//
// White noise, unaltered by rate. ie scale-free
//
#include "PhyaPluginPrivatePCH.h"


#include "Utility/paRnd.hpp"
#include "Contact/paFunContactGen.hpp"
#include "Contact/paWhiteFun.hpp"

void
paWhiteFun :: tick(paFunContactGen* gen){

	paFloat* out = gen->getOutput()->getStart();
	int i;

	for(i = 0; i < paBlock::nFrames; i++)
		out[i] = paRnd(-1.0f, 1.0f);

}



