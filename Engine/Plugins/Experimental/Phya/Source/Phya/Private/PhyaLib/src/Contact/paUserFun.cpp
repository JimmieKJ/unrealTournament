//
// paUserFun.cpp
//
// Template for adding user surface height function, eg from graphics position.
//
#include "PhyaPluginPrivatePCH.h"

#include "System/paConfig.h"

#if USING_USER_FUN
#include "SignalProcess/paBlock.hpp"
#include "Contact/paUserFun.hpp"
#include "Contact/paFunContactGen.hpp"

#include <math.h>



void
paUserFun :: tick(paFunContactGen *gen){

	paFloat* out = gen->getOutput()->getStart();
	paFloat x = gen->m_x;
	paFloat r = gen->m_rate;
	int i;


	for(i = 0; i < paBlock::nFrames; i++)
	{
		x += r;
//		out[i] =			// Add user height generating code here.
	}

	gen->m_x = x;			// Save state.
}


#endif // USING_USER_FUN




