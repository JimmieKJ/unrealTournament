//
// paBlock.cpp
//
// Container class for audio blocks used in signal processing.
//
//
#include "PhyaPluginPrivatePCH.h"

#include "System/paConfig.h"
#include <limits.h>
#include "Signal/paBlock.hpp"
#include "System/paMemory.hpp"
#include "System/paRand.hpp"


paObjectPool<paBlock>
paBlock :: pool;

#define		INIT_NBLOCKFRAMES 128
int			paBlock::nMaxFrames = INIT_NBLOCKFRAMES;
int			paBlock::nFrames = INIT_NBLOCKFRAMES;

paBlock*
paBlock :: newBlock()
{
	paBlock* b = pool.newActiveObject();
	if (b != 0) b->m_poolHandle = pool.getHandle();
	return b;
}


int
paBlock :: deleteBlock(paBlock* b)
{
	return pool.deleteActiveObject(b->m_poolHandle);
}


paBlock :: paBlock()
{
	m_samples = (paFloat*)paCalloc(nMaxFrames, sizeof(paFloat));
	m_start = m_samples;
//	m_zeroState = false;
	m_poolHandle = -1;
}

paBlock :: ~paBlock()
{
	paAssert(nFrames <= nMaxFrames);
	paFree(m_samples);
}


void
paBlock :: zero()
{
//	m_zeroState = true;

	paAssert(nFrames <= nMaxFrames);
	int i;
	for(i = 0; i < nFrames; i++) m_start[i] = 0;

}


void
paBlock :: copy(paBlock* input)
{
//	m_zeroState = false;
	paMemcpy( m_start, input->m_start, sizeof(paFloat)* nFrames );
}


void
paBlock :: multiplyBy(paFloat multfactor)
{
	int i;
	for(i=0; i<nFrames; i++) m_start[i] *= multfactor;		// This should be wrapped up after compilation.
}


void
paBlock :: add(paBlock* inBlock)
{
	int i;

	paAssert(inBlock != 0);
//	if (m_zeroState) { copy(inBlock); m_zeroState = false; return; };
	paFloat* in = inBlock->m_start;
	for(i=0; i<nFrames; i++) m_start[i] += in[i];
}

void
paBlock :: addWithMultiply(paBlock* inBlock, paFloat multfactor)
{
	int i;

//	if (m_zeroState) { copyWithMultiply(inBlock, multfactor); return; };

	if (multfactor !=0)
	{
		paFloat* in = inBlock->m_start;
		for(i=0; i<nFrames; i++) m_start[i] += in[i] * multfactor;
	}
}


void
paBlock :: copyWithMultiply(paBlock* inBlock, paFloat multfactor)
{
	int i;

//	if (multfactor == 0) {
//		m_zeroState = true;
//		return;
//	}
//	m_zeroState = false;

	paFloat* in = inBlock->m_start;
	for(i=0; i<nFrames; i++) m_start[i] = in[i] * multfactor;
}



int
paBlock :: setnMaxFrames(int n)
{
	if (pool.getnObjects() != 0) return -1; // Can't change now.

	nMaxFrames = n;
	nFrames = n;
	return 0;
}

int
paBlock :: setnFrames(int n)
{
	if (n > nMaxFrames) return -1;

	nFrames = n;
	return 0;
}




void
paBlock :: square()
{
	int i;
	for(i=0; i<nFrames; i++) m_start[i] = m_start[i]*m_start[i];
}



void
paBlock :: fillWithNoise()
{
	int i;
//	paFloat shift = paRAND_MAX * .5;	// NB this gives true zero average.
//	for(i=0; i<nFrames; i++) m_start[i] = (paFloat)paRand() - shift;
	for(i=0; i<nFrames; i++) m_start[i] = paRnd((paFloat)-1.0, (paFloat)1.0);

}


void
paBlock :: multiplyByNoise()
{
	int i;
//	paFloat shift = paRAND_MAX * .5;	// NB this gives true zero average.
	for(i=0; i<nFrames; i++)  m_start[i] *= paRnd((paFloat)-1.0, (paFloat)1.0);
}


void
paBlock :: fillWithNoise(paFloat amp)
{
	int i;
//	paFloat scale = 2*amp/paRAND_MAX;
//	for(i=0; i<nFrames; i++) m_start[i] = -amp + scale * (paFloat)paRand();
	for(i=0; i<nFrames; i++) m_start[i] = paRnd(-amp, amp);
}



paFloat
paBlock :: sum()
{
	paFloat sum = 0;

	//! This isn't the most numerically-accurate method:

	int i;
	for(i=0; i<nFrames; i++)
		sum += m_start[i];

	return sum;
}


void
paBlock :: fadeout()
{
	int i;
	paFloat incr = (paFloat)-1/nFrames;
	paFloat mult = (paFloat)1.0;
	for(i=0; i<nFrames; i++)
	{
		m_start[i] *= mult;
		mult += incr;
	}
}


void
paBlock :: limit()
{
	int i;
	//paFloat a = SHRT_MAX*0.75;
	//paFloat b = SHRT_MAX;
	//paFloat c = b-a;

	for(i=0; i<nFrames; i++)
	{
		if (m_start[i] > SHRT_MAX) m_start[i] = SHRT_MAX;
		else if (m_start[i] < SHRT_MIN) m_start[i] = SHRT_MIN;

/*
		if (m_start[i] > a)
		{
			m_start[i] = b - c*c/(m_start[i]-a+c);
		}
		else if (m_start[i] < -a)
		{
			m_start[i] = -b + c*c/(-m_start[i]-a+c);
		}
*/
	}
}


void
paBlock :: plot(paFloat s)
{
	int i, x, c;

	for(i=0; i<nFrames; i++)
	{
		x = (int) (m_start[i]*s *40.0 +40.0);

		if (x >= 78) x = 78;
		if (x < 0) x = 0;

		for(c=0; c < x; c++) printf(" ");
		printf("+\n");
	}

}

void
paBlock :: plot()
{
	plot(1.0f);
}
