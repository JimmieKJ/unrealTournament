//
// paRndFun.cpp
//
// Simple, efficient random surface functions.
// These are stochastic rather than consistently repeating when position reoccurs.
//
//! Future - Would like repeating random functions aswell, using indexed random variables.

#include "PhyaPluginPrivatePCH.h"

#include "Utility/paRnd.hpp"
#include "Contact/paFunContactGen.hpp"
#include "Contact/paRndFun.hpp"


bool ghit = true;

void
paRndFun :: tick(paFunContactGen* gen){

	paFloat* out = gen->getOutput()->getStart();
	paFloat r = gen->m_rate / paScene::nFramesPerSecond; //! Per rate adjust. Better to derive GridSurface and overide SetRateAtSpeed to include per sample factor.

	paFloat zr;
	paFloat y;
	int i;

	y = gen->m_y;
	if (r<0) r = -r;
	zr = r * m_zeroRate;		// As rate increases, zero_rate increases and size of bumps decreases.
	
//if (ghit) m_zr = paRnd(10000.0f, 10000.0f) / paScene::nFramesPerSecond;		// Fixed width for foil.

//	gen->getOutput()->fillWithNoise();		//! expt. noise modulation.

	ghit = false;
	for(i = 0; i < paBlock::nFrames; i++)
	{

		if (paRnd(1.0f) < r)		//! More efficient to calculate time to next bump once at each bump.
		{
			y = paRnd(m_min, 1.0f);
			ghit = true;			// expt. Flag new hit to other signal processes, eg FOIL filter change
		}
		else if (m_fastZero || paRnd(1.0f) < zr) y = 0.0f;	

//		out[i] = y;
		if (y != 0.0f) out[i] = paRnd(y);    else out[i] = 0.0f;

//		out[i] *= y*0.001;

	}

	gen->m_y = y;
}



