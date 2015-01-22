//
// paFunImpactGen.cpp
//
#include "PhyaPluginPrivatePCH.h"

#include "System/paConfig.h"
#include <math.h>
#include "Impact/paFunImpactGen.hpp"
#include "Signal/paBlock.hpp"
#include "Surface/paFunSurface.hpp"

//#include <stdio.h>

paFunImpactGen::paFunImpactGen()
{
	m_isQuiet = true;
}



void
paFunImpactGen::initialize(paFunSurface* surface)
{
	// Copy / reference surface data.

	m_isNew = true;							
	m_surface = surface;
	m_otherSurface = 0;
   	m_contactGen.initialize(surface);		//!! Wasteful to always have skid contactgen with each impact. Probably its not used.
	m_limit = surface->m_impactAmpMax;

	m_lowpass.reset();

	if (m_surface->m_nImpactSamples > 0)
	{
		if (m_surface->m_nImpactSamples == 1) m_samplePlaying = 0;
		else
		{
			int lastSample = m_samplePlaying;
			m_samplePlaying = paRnd(0, m_surface->m_nImpactSamples -2);
			if (m_samplePlaying >= lastSample) m_samplePlaying += 1;  // Don't repeat sample.
		}
		m_endOfSample = false;
	}
	else m_endOfSample = true;	// 
}




paBlock*
paFunImpactGen::tick()
{
	if (m_isNew)		// Wait for first dynamic data before setting up impact sound.
	{
		paFloat hardness = m_surface->m_hardness;

		// Add extra hardness from nonlinearity of impact.
		if (m_impactImpulse > m_surface->m_impulseToHardnessBreakpoint) {
			hardness += m_surface->m_impulseToHardnessScale * (m_impactImpulse - m_surface->m_impulseToHardnessBreakpoint);
		}

		if (m_surface->m_maxHardness > 0.0f)
			if (hardness > m_surface->m_maxHardness) hardness = m_surface->m_maxHardness;
		//! When hardness limit is reached use an extra random factor to reduce repetition effect.

		// Find the least hard surface, if there are two.
		// (This is a simplification of impulseTime = sqrt((1/l1+1/l2)/(1/m1+1/m2))
		// (l1,l2 are spring constants for the surfaces ie 'hardnesses'.
		// Could incorporate body mass from body data accessed through paImpact.

		if (m_otherSurface)
		{
			paFloat otherSurfaceHardness = m_otherSurface->m_hardness;

			if (m_impactImpulse > m_otherSurface->m_impulseToHardnessBreakpoint) {
				otherSurfaceHardness += m_otherSurface->m_impulseToHardnessScale * (m_impactImpulse - m_otherSurface->m_impulseToHardnessBreakpoint);
			}

			if (otherSurfaceHardness < hardness)
				hardness = otherSurfaceHardness;
		}

// printf("%f\n",hardness);


		m_amp = m_impactImpulse * m_impactGain;	//! sqrt(E/2k)

		if (m_limit > 0){
			if (m_amp > m_limit) m_amp = m_limit;
		}

		if (m_surface->m_nImpactSamples == 0)
		{
			m_pulser.setWidthSeconds(1/hardness);		//! sqrt(m/k)
			m_pulser.hit(m_amp);
		}
		else
		{
			m_x = 0;
			m_endOfSample = false;
			m_lowpass.setCutoffFreq(hardness);
		}

		m_isSkidding = false;

		if ((m_surface->m_skidGain != 0.0)
			&& ( (m_surface->m_skidThickness >0) || (m_surface->m_nSkidBlocks >0) )
			&& (m_relTangentSpeedAtImpact != 0.0))
		{
			m_isSkidding = true;
			m_contactGen.setGain(m_surface->m_skidGain);
			m_contactGen.setSpeedBody1RelBody2(m_relTangentSpeedAtImpact);
			m_contactGen.setSpeedContactRelBody(m_relTangentSpeedAtImpact);
			m_contactGen.setContactForce(m_impactImpulse * m_surface->m_skidImpulseToForceRatio);

			if (m_surface->m_skidThickness > 0)
			{
				paFloat skidTime = m_surface->m_skidThickness / m_relNormalSpeedAtImpact;
				m_surface->setSkidTime(skidTime);		// Calc nBlocks.
			}
			m_skidCount = m_surface->m_nSkidBlocks;
		}

		m_isNew = false;
	}


	if (m_surface->m_nImpactSamples == 0)
		m_pulser.tick();
	else
	{
		int i;
		paFloat* const o = (paFloat* const)m_output->getStart();
		short* const s = (short* const)m_surface->m_start[m_samplePlaying];
		const long nf = m_surface->m_nFrames[m_samplePlaying];

		if (!m_endOfSample)
		{
			for(i = 0; i < paBlock::nFrames && m_x < nf; i++, m_x++)
				o[i] = (paFloat)s[m_x] * m_amp;
				
			if (m_x == nf)
			{
				m_endOfSample = true;
				for(; i < paBlock::nFrames; i++)
					o[i] = 0.0f;
			}
		}
		else
			for(i=0 ; i < paBlock::nFrames; i++)
				o[i] = 0.0f;



//		m_lowpass.tick();
	}


	if (m_isSkidding) {
		// Get temporary audio block for contact calculation.
		paBlock* contactOutput = paBlock::newBlock();
		m_contactGen.setOutput(contactOutput);
		m_contactGen.tick();
		m_output->add(contactOutput);
		paBlock::deleteBlock(contactOutput);
		m_skidCount--;
		if (m_skidCount == 0) m_isSkidding = false;
	}

	// Impact quiet when pulser quiet and skid quiet.

	m_isQuiet = (m_pulser.isQuiet() && (!m_isSkidding) && (m_endOfSample));
//	m_isQuiet = (m_endOfSample);

	return m_output;
}


bool
paFunImpactGen::isQuiet()
{
	return m_isQuiet;
}
