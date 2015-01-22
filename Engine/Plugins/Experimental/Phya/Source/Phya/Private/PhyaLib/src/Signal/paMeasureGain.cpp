//
// paMeasureGain.cpp
//
// A tool for measuring the broad-band average gain of a filter, eg a resonator.
// Can be used for prenormalising a filter.
//
#include "PhyaPluginPrivatePCH.h"

//#include <math.h>
#include "Signal/paMeasureGain.hpp"
#include "Signal/paBlock.hpp"
#include "Signal/paLowpass.hpp"
#include "Scene/paAudio.hpp"


paFloat
paMeasureGain( void (*filter)(paBlock* input, paBlock* output) )
{

	int nTicks;
	int nMaxTicks;
	paFloat sampleTime = 1.0f;		// This ensures good response down to 10Hz.

	paFloat sigSum = 0;
	paFloat refSum = 0;

	paBlock ref;
	paBlock sig;

	nMaxTicks = int(sampleTime * (paFloat)paScene::nFramesPerSecond / (paFloat)paBlock::nFrames);


	for(nTicks=0; nTicks<nMaxTicks; nTicks++)
	{
		ref.fillWithNoise();

		filter(&ref, &sig);
		sig.square();
		sigSum += sig.sum();		// Add sum of all samples.

		ref.square();
		refSum += ref.sum();
	}


	paFloat gain = (paFloat)sqrt(sigSum/refSum);

	return gain;

}
