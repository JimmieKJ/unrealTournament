//
// paTriPulser.cpp
//
// Generates a triangular pulse with controllable width and amp.
// NB the output will be slightly aliased because of the highfreq content due to the non-smooth points.
//
//
#include "PhyaPluginPrivatePCH.h"

#include "System/paConfig.h"
#include "Signal/paTriPulser.hpp"
#include "System/paMemory.hpp"


paTriPulser :: paTriPulser()
{
	m_beenHit = false;
	m_isQuiet = true;	
	m_widthSecs = (paFloat)0.001;		// Sensible default width 1 millisecond.
	m_widthSamps = (int)(m_widthSecs * paScene::nFramesPerSecond); 
	if (m_widthSamps > paBlock::nFrames) {
		m_widthSamps = paBlock::nFrames;
		m_widthSecs = (paFloat)m_widthSamps / paScene::nFramesPerSecond;
	}
}


paTriPulser :: ~paTriPulser()
{
}

int 
paTriPulser :: setWidthSeconds(paFloat width)
{
	m_widthSecs = width;
	m_widthSamps = (int)(width * paScene::nFramesPerSecond); 
	if (m_widthSamps == 0) m_widthSamps = 2;
	if (m_widthSamps > paBlock::nFrames) 
	{
		m_widthSamps = paBlock::nFrames-1;
		return -1;
	}
	return 0;
}


int
paTriPulser :: hit(paFloat a)
{
	if (m_beenHit) return -1;	// Otherwise quieter hits could kill the initial big hit.

	m_amp = a;	
	m_beenHit = true;

	return 0;
}


paBlock*
paTriPulser :: tick()
{
	if (!m_beenHit)  {
		m_output->zero();	// Should have a zero flag.
		return m_output;
	}

	paFloat* out = m_output->getStart();
	m_beenHit = false;
	m_isQuiet = false;

	// The peak value attained is adjusted to make different 
	// widths sound similar in loudness.

	int freeSamps = paBlock::nFrames - m_widthSamps;
	paAssert(freeSamps>=0);

	paFloat pulseSamp = 0;
	paFloat rate;
	int t = 0;
	int i = 0;


	// Randomizing the onset of the pulse prevents 'machine gun' - like effects
	// when many pulses are supposed to be occuring randomly.
	// Need to add random predelay of length order main-loop tick,
	// ie several audio-loop ticks.

//	if (freeSamps>0)
//	{
//		t = paRnd(0, freeSamps);
//		while(i<t) {				// Zero
//			out[i] = 0;
//			i++;
//		}
//	}

	t+= m_widthSamps/2;
	rate = m_amp/(m_widthSecs * t); 

	while(i<t) {					// Up
		out[i] = pulseSamp;
		pulseSamp += rate;
		i++;
	}

	t+= m_widthSamps/2;
	while(i<t) {					// Down
		out[i] = pulseSamp;
		pulseSamp -= rate;
		i++;
	}

	while(i<paBlock::nFrames) {	// Zero
		out[i] = 0;
		i++;
	}

	// If a pulse could cross several blocks then 
	// m_isQuiet would be false across blocks.

	m_isQuiet = true;	

	return m_output;
	 
}







// Not finished- will allow impacts asynchronously across block boundaries.
// Not very useful because block size is usually large enough for a complete hit anyway.
/*
	int i;
	if (rate == 0) return nullOutput;
	int sampsToTarget = (target-out)/rate;

	if (sampsToTarget > nBlockSamples)  sampsToTarget = nBlockSamples;

	for(i=0; i<sampsToTarget; i++) {
		out+= rate;
		output[i] = out;
	}

	if (i == nBlockSamples) {
		

		return output;
	}
*/
	
	

