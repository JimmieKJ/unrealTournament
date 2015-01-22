//
// paGridFun.cpp
//
// Grid like excitation.
// - It repeats consistently when r reverses.
// - The discontinuities remain hard even at low rates.
// This is an approximation to resampling a very high initial bandwidth,
// And can generate more realistic surface excitations than linear resampling.

#include "PhyaPluginPrivatePCH.h"

#include "Signal/paBlock.hpp"
#include "Contact/paFunContactGen.hpp"
#include "Contact/paGridFun.hpp"

#include <math.h>



void
paGridFun :: tick(paFunContactGen *gen){

	paFloat* out = gen->getOutput()->getStart();
	paFloat x = gen->m_x;
	paFloat r = gen->m_rate /paScene::nFramesPerSecond; //! Per rate adjust. Better to derive GridSurface and overide SetRateAtSpeed to include per sample factor.
	int i;
	

	for(i = 0; i < paBlock::nFrames; i++)
	{
		x += r;
		out[i] = ((float)( (int)(x-(float)(int)x - m_cut) )-0.5f);
//		out[i] = ((float)(int(x) % 2)-0.5f);		// Even mark/space
	}

	gen->m_x = x;			// Save state.
}






