//
// paFunContactGen.cpp
//
#include "PhyaPluginPrivatePCH.h"

#include "System/paConfig.h"
#include "Contact/paFunContactGen.hpp"
#include "Signal/paBlock.hpp"
#include "Surface/paFunSurface.hpp"

paFunContactGen::paFunContactGen()
{
}


void
paFunContactGen::initialize(paFunSurface* surface)
{
	// Copy / reference surface data.

	m_surface = surface;
	paAssert(surface->getFun());
	m_x = m_y = 0.0f;
//	m_x = paRnd(0,100000);		//! Hack. Random start position for wav loop. Better to build WavSurface class. 
//	m_x = 100;
	m_rate = 0.0f;

	m_lowpass.reset();
	m_lowpass2.reset();
	m_lowpass3.reset();
	m_limit = surface->m_contactAmpMax;
	m_isActive = true;
	m_out = 0.0f;
	m_cfF = 0.0f;
	m_scF = 0.0f;
	m_ssF = 0.0f;

#ifdef FREEPARTICLESURF
	m_hitLowpass.reset();
	m_particleBiquad.reset();
	m_hitLowpass.setGain(1.0f);
#endif
#ifdef GRAVEL1
	m_hitLowpass.setCutoffFreq(7.0f);		// Set hit decay time.
	m_particleBiquad.setCoeffs(8.552857f,  -14.73601f,  7.329785f,  -0.525672f,  0.672305f);		// Gravel
#endif
#ifdef GRAVEL2
	m_hitLowpass.setCutoffFreq(7.0f);		// Set hit decay time.
	m_particleBiquad.setCoeffs(6.913595,   -12.9,  6.276372,  -1.484727,  0.774768);		// Gravel
#endif
#ifdef FOIL
	m_hitLowpass.setCutoffFreq(7.0f);		// Set hit decay time.
	m_particleBiquad.setCoeffs(0.379378f,  -0.758755f,  0.379378f,  -0.973297f,  0.32546f);		// Gravel
#endif
#ifdef REEDS
	m_hitLowpass.setCutoffFreq(1.2f); //0.6		// Set hit decay time.
	m_particleBiquad.setCoeffs(1.572948, -3.145896,  1.572948,  -1.512409,  0.629712);		// Gravel
#endif
}



extern bool ghit;
paFloat cutoffFreqOffset = 0.0f;

paBlock* 
paFunContactGen::tick()
{
	paFloat alpha = m_surface->m_systemAlpha;
	m_cfF = alpha * m_contactForce + (1 - alpha) * m_cfF;

	m_amp = m_cfF * m_gain;
//	if (m_speedBody1RelBody2 < 0) m_speedBody1RelBody2 = -m_speedBody1RelBody2;
	if (m_speedBody1RelBody2 < m_surface->m_gainBreak) 
		m_amp *= (m_surface->m_gainAtRoll + m_surface->m_gainRate * m_speedBody1RelBody2);

//	if (m_amp < m_surface->m_contactAmpMin) m_amp = m_surface->m_contactAmpMin;
	m_amp += m_surface->m_contactAmpMin;

//	if (m_limit > 0) { if (m_amp > m_limit) m_amp = m_limit; }
	if (m_surface->m_contactAmpMax > 0) 
	   if (m_amp > m_surface->m_contactAmpMax) m_amp = m_surface->m_contactAmpMax;

	// Lowpass filter the contact speeds to simulate loose particle energy decay.

//m_speedBody1RelBody2 += paRnd(-100.0f, 100.0f);	// Random fluctuation can be good.
	m_ssF =  alpha * m_speedBody1RelBody2 + (1 - alpha) * m_ssF;
	
	paFloat cutoffFreq = m_surface->m_cutoffMin + m_ssF * m_surface->m_cutoffRate;
	if (m_surface->m_cutoffMax > 0)
		if (cutoffFreq > m_surface->m_cutoffMax) 	cutoffFreq = m_surface->m_cutoffMax;

//if (cutoffFreq < m_surface->m_cutoffMin) cutoffFreq = m_surface->m_cutoffMin;

	m_scF =  alpha * m_speedContactRelBody + (1 - alpha) * m_scF;

	m_rate = m_scF * m_surface->m_rateScale + m_surface->m_rateMin;
	if (m_surface->m_rateMax >0)
		if (m_rate > m_surface->m_rateMax) m_rate = m_surface->m_rateMax;

//printf("%f %f\n", m_amp, cutoffFreq);
//printf("%f %f\n", m_amp, m_speedBody1RelBody2);	

	m_surface->m_fun->tick(this);		// Use state from this gen.
//m_output->zero();


#ifdef FOIL
if (ghit)
{
	if (paRnd(0.0f, 1.0f) < 0.3) cutoffFreqOffset = paRnd(0.0f, 2500.0f);	// 'Spectral mobility'
}
	m_hitLowpass.setCutoffFreq(2.0f + paRnd(0.0f, 5.0f));	// Change length of noise pulse.

	cutoffFreq += cutoffFreqOffset;
// Change length of initial pulse in paRndFun
#endif



#ifdef FREEPARTICLESURF
	m_hitLowpass.tick();
//	m_output->plot(100.0);
	m_output->multiplyByNoise();
	m_particleBiquad.tick();
#endif


//m_output->fillWithNoise();




//m_amp = 90000.0;
//cutoffFreq = 20000; //7000.0; //4860.0; 20000.0;


	m_lowpass.setCutoffFreq(cutoffFreq);
	m_lowpass.setGain(m_amp);
	m_lowpass.tick();
//	m_output->plot(0.01);

	#ifdef FREEPARTICLESURF
	m_lowpass2.setCutoffFreq(cutoffFreq);	//! redundant coeff calc. redundant gain mult. Copy from lowpass1 / interp gain class.
	m_lowpass3.setCutoffFreq(cutoffFreq);
	m_lowpass2.tick();
	m_lowpass3.tick();
	#endif


//
//	if (m_surface->m_cutoffMin2 > 0)
//	{
//		paFloat cutoffFreq2 = m_surface->m_cutoffMin2 + m_speedBody1RelBody2 * m_surface->m_cutoffRate2;
//		if (cutoffFreq2 > m_surface->m_cutoffMax2) cutoffFreq2 = m_surface->m_cutoffMax2;
////cutoffFreq2 = 50.0;
//		m_lowpass2.setCutoffFreq(cutoffFreq2);
//		m_lowpass2.setGain(1.0);
//
//		m_lowpass2.tick();				// 2nd order lowpass.
//
//	}

//	m_output->zero();
	m_out = m_output->getLastSample();
	return m_output;
}

#ifndef ABS
#define ABS(A) ((A>0) ? A : -A)
#endif


//!! For more complex gens may be neccessary to fade out generator
bool
paFunContactGen :: isQuiet()
{
// Problem with using force : In Bullet contacts can suddenly jump to zero force during movement.
if (m_amp <= 0.000000001 && m_ssF <= m_surface->m_quietSpeed)	//!!	
{
 	return true;
}
//	if (m_contactForce == 0 || ABS(m_speedContactRelBody) < m_surface->m_quietSpeed) return true;

//printf("%f\n",m_contactForce);

//	if (m_speedContactRelBody + m_speedBody1RelBody2 <= m_surface->m_quietSpeed)
////		&& m_contactForce <  0.001);	// Just for testing wavfun- need to get energy decay into funcontactgen.
////		&& ( ABS(m_out) < 0.00001) )		//! Need to detect gen energy not instant waveform amp (cf res energy).
//	{
//		return true;
//	}
return false;
};


bool 
paFunContactGen :: isActive()
{ 
	if (m_isActive) return true;
	else if (m_speedContactRelBody + m_speedBody1RelBody2 > m_surface->m_quietSpeed)
	{ m_isActive = true; return true; }		// Turn gen back on.
	return false;
};