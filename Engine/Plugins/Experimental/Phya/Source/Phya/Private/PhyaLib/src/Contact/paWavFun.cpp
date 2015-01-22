//
// paWavFun.cpp
//
// Wav like excitation.
// NB - it repeats consistently when rate reverses.
//
#include "PhyaPluginPrivatePCH.h"


#include "System/paConfig.h"
#include "System/paConfig.h"

#if USING_AIO
#include "AIO.hpp"	// Use external lib sound file load function.
#endif // USING_AIO

#include "Signal/paBlock.hpp"
#include "Contact/paFunContactGen.hpp"
#include "Contact/paWavFun.hpp"

#include <math.h>


int
paWavFun::readWav(char* filename)
{

#if USING_AIO

	CAIOwaveConfig config;

	if (m_start != NULL) paFree(m_start);

	if (AIOloadWaveFile(filename, &config) == -1) return -1;

	// Make sure its in the required format.
	if (config.m_nBitsPerMonoSample != 16) return -1;
	if (config.m_nChannels != 1) return -1;


	m_nFrames = config.m_nFrames;
	m_start = (short*)paMalloc( (m_nFrames+1) * sizeof(short) );	// Reserve +1 for unwrapped start[0].

	paMemcpy( m_start, config.m_start, sizeof(short)* m_nFrames );
	AIOfreeWaveFile();


	m_start[m_nFrames] = m_start[0];	// Unwrapped value added for efficient interpolation.
										// (more unwrapped values would be required for higher order interpolation.)

#endif // USING_AIO

	return 0;
}


void
paWavFun::freeWav()
{
	paFree(m_start);
}


void
paWavFun :: tick(paFunContactGen *gen){

	paFloat* out = gen->getOutput()->getStart();
	paFloat x = gen->m_x;
	paFloat r = gen->m_rate;
	paFloat y;
	paFloat a;
	paFloat n = (paFloat)m_nFrames;
	int i;
	int wi;

	paAssert(m_start);

	if (m_interp)
	for(i = 0; i < paBlock::nFrames; i++) {

		wi = (long)x;
		//! This instruction can cost an unreasonable amount of time on i386 machines, may want to use fixed point.

		a = x - (paFloat)wi;			// calculate the fractional part of the position.
		// Linear interpolation between two successive samples. This is slighty
		// rearranged for speed from: out = (1-a) * wav[wi] + a * wav[wi+1]

		y = (paFloat)m_start[wi];
		out[i] = a * ((paFloat)m_start[wi+1] - y) + y;

		x += r;
		if (x >= n) x -= n;			// Wrap play position.
		else if (x < 0) x += n;
	}

	else
	for(i = 0; i < paBlock::nFrames; i++) {
		wi = (long)x;
		//! This instruction can cost an unreasonable amount of time on i386 machines, may want to use fixed point.

		out[i] = (paFloat)m_start[wi];	//	No interpolation.

		x += r;
		if (x >= n) x -= n;					// Wrap play position.
		else if (x < 0) x += n;

	}

	gen->m_x = x;			// Save state.
}


//! Should add support for rate = 1 const, integers only.





