//
// paFunSurface.cpp
//
// Surface using surface functions, paFun.
// Output is lowpass filtered according to relative body speed at contact.
//

#include "PhyaPluginPrivatePCH.h"

#include "Surface/paFunSurface.hpp"
#include "Contact/paFunContactGen.hpp"
#include "Impact/paFunImpactGen.hpp"


paFunSurface :: paFunSurface()
{
	setCutoffFreqAtRoll((paFloat)70.0);
	setCutoffFreqRate((paFloat)8000.0);
	setCutoffFreqMax((paFloat)-1.0);
	setCutoffFreq2AtRoll((paFloat)-1.0);
	setCutoffFreq2Rate((paFloat)80000.0);
	setCutoffFreq2Max((paFloat)-1.0);

	setRateMin(0.0f);
	setRateMax(-1.0f);
	setRateAtSpeed((paFloat)1.0, 1.0);

	setSystemDecayAlpha((paFloat)1.0);
	setGainBreakSlipSpeed(0.0);
	setGainAtRoll(1.0);
	setQuietSpeed((paFloat)0.0);

	setSkidImpulseToForceRatio((paFloat).01);	//! Would like normalised to 1.0
	setSkidGain((paFloat)1.0);
	setSkidTime((paFloat)0.0);				// No auto skid initially. Auto skidding is a lower-quality technique..
	setSkidThickness((paFloat)-1.0);
	setSkidMinTime((paFloat)-1.0);
	setSkidMaxTime((paFloat)-1.0);
	setHardness((paFloat)1000.0);
	m_nImpactSamples = 0;
}

paFunSurface :: ~paFunSurface()
{
	for( int index = 0; index < m_nImpactSamples; ++index )
		paFree(m_start[index]);
}

paObjectPool<paFunContactGen> paFunSurface :: contactGenPool;
paObjectPool<paFunImpactGen> paFunSurface :: impactGenPool;


//! Possible improvement by defining newContactGen in Surface.cpp
// and replacing contactGenPool.newActiveObject() with m_contactGenPool->newActiveObject()
// and defining  m_contactGenPool = &contactGenPool in funSurface().
// Worth changing when there are more than one type of surface.


paContactGen*
paFunSurface :: newContactGen(){
	paFunContactGen* gen = contactGenPool.newActiveObject();

	if (gen == 0) return 0;		//error -no gens left.

	gen->m_poolHandle = contactGenPool.getHandle();

	gen->initialize(this);

	return gen;
};


int
paFunSurface :: deleteContactGen(paContactGen* gen) {
	/*int err = */
	if (gen == 0) return -1;
	assert(gen->m_poolHandle != -1);
	if( gen->m_poolHandle != -1 )	//!! Should never be -1 if gen exists.
		contactGenPool.deleteActiveObject(gen->m_poolHandle);

	gen->m_poolHandle = -1;

	return -1;
}





paImpactGen*
paFunSurface :: newImpactGen(){

	paFunImpactGen* gen = impactGenPool.newActiveObject();

	if (gen == 0) return 0;		//error -no gens left.

	gen->m_poolHandle = impactGenPool.getHandle();

	gen->initialize(this);

	return gen;
};


int
paFunSurface :: deleteImpactGen(paImpactGen* gen) {
	/*int err = */
	if (gen == 0) return -1;
	impactGenPool.deleteActiveObject(gen->m_poolHandle);
	gen->m_poolHandle = -1;
	return -1;
}


int
paFunSurface :: setSkidTime(paFloat time) { // time in seconds.
	if (m_skidMinTime >0.0)
		if (time<m_skidMinTime) time = m_skidMinTime;
	if (m_skidMaxTime >0.0)
		if (time>m_skidMaxTime) time = m_skidMaxTime;

	if (time >0.0)
		// Skid for atleast one block of time.
		m_nSkidBlocks =
		(int)( (float)paScene::nFramesPerSecond / (float)paBlock::nFrames * time) + 1;
	else
		m_nSkidBlocks = 0;
	return 0;
}


//! todo
//int
//paFunSurface::setnImpactSamples(int n)
//{
//}

int
paFunSurface::setImpactSample(short* start, long nFrames)
{
	if (m_nImpactSamples >= 6) return -1; // No more sample slots.
	m_nFrames[m_nImpactSamples] = nFrames;
	m_start[m_nImpactSamples] = (short*)paMalloc( (nFrames) * sizeof(short) );
	paMemcpy( m_start[m_nImpactSamples], start, sizeof(short)* nFrames );
	m_nImpactSamples++;

	return 0;
}


#if USING_AIO

#include "AIO.hpp"	// Use external lib sound file load function.


//int
//paFunSurface::setImpactSample(char* filename)
//{
//	CAIOwaveConfig config;
//
//	if (m_start != NULL) paFree(m_start);
//
//	if (AIOloadWaveFile(filename, &config) == -1) return -1;
//
//	// Make sure its in the required format.
//	if (config.m_nBitsPerMonoSample != 16) return -1;
//	if (config.m_nChannels != 1) return -1;
//
//
//	m_nFrames = config.m_nFrames;
//	m_start = (short*)paMalloc( (m_nFrames) * sizeof(short) );
//
//	paMemcpy( m_start, config.m_start, sizeof(short)* m_nFrames );
//	AIOfreeWaveFile();
//
//	return 0;
//}



int
paFunSurface::setImpactSample(char* filename)
{
	CAIOwaveConfig config;

//	if (m_start != NULL) paFree(m_start);

	if (AIOloadWaveFile(filename, &config) == -1) return -1;

	// Make sure its in the required format.
	if (config.m_nBitsPerMonoSample != 16) return -1;
	if (config.m_nChannels != 1) return -1;

	setImpactSample(config.m_start, config.m_nFrames);

	AIOfreeWaveFile();

	return 0;
}



#endif