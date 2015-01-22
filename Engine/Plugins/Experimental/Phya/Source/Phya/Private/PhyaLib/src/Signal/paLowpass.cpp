//
// paLowpass.cpp
//
#include "PhyaPluginPrivatePCH.h"


#include "Signal/paLowpass.hpp"

//#include <math.h>

paFloat paLowpass::twoPiDivFrameRate = 0;
paFloat paLowpass::twoDivFrameRate = 0;


paLowpass::paLowpass()
{
	m_input = 0;
	m_output = 0;

	// Initially no filtering.
	m_g_old = 0.0;	// Quick fade on at start
	m_g = 1.0;
	m_b_old = 0.0f;
	m_y = 0.0f;

	m_updateCutoffFreq = false;

	twoPiDivFrameRate = (paFloat)2.0f * 3.1459265f / paScene::nFramesPerSecond;		//!! Don't want to calc this for every Lowpass : put in scene.
	twoDivFrameRate = (paFloat)2.0f / paScene::nFramesPerSecond;
}

paLowpass::~paLowpass()
{
}


paBlock*
paLowpass::tick()
{

	// Catch control variable changes.
	// NB need event list to reproduce changes in the correct order.

	// This cutoff-to-pole calc comes from Csound. It looks a bit dodgey because aliased
	// cutoffs make the pole wrap around: It should be meaningful to make the pole shrink
	// to zero so that the filter becomes a bypass in the limit of high cutoff.

	if (m_updateCutoffFreq) {

		paFloat f = m_cutoffFreq * twoPiDivFrameRate;

		if (f > (paFloat)3.142659) f = (paFloat)3.142659;
		
		if (f > 0.001)
		{
			paFloat t = 2.0f - (paFloat)cos(f);
			m_b = 1 - (1-(t - (paFloat)sqrt(t*t-1))) * 1.2071f;	// warped to reach full pole range. //! speed-up.
		}
		else m_b = 1 - f * 1.2071f;	//	small f approx, when above fails, eg for dc blocks


		//paFloat f = m_cutoffFreq * twoPiDivFrameRate;
		//if (f > 3.1415926) m_b = 0.0f;
		//else m_b = 1 - sin(m_cutoffFreq * twoPiDivFrameRate);	// cutoffFreq = FrameRate/4 -> a=1 b=0   : maxmsp Hz

//		m_b = 1 - m_cutoffFreq * twoPiDivFrameRate;		// cutoffFreq = FrameRate/2pi -> a=1 b=0  : Same low freq behavour as maxmsp, finishes faster.
//		m_b = 1 - m_cutoffFreq * twoDivFrameRate;	// cutoffFreq = FrameRate/2 -> a=1 b=0 : rational
//		if (m_b < 0) m_b = 0;

//		m_b = exp(-0.5*f);	// music-dsp

	}


	m_updateCutoffFreq = false;

	paFloat* x = m_input->getStart();
	paFloat* y = m_output->getStart();
	// NB x=y is ok, but obviously the contents of x are then lost.

	int i;



	// Combine filter coefficient with amplitude envelope on input:
//	paFloat a = m_g_old * (1 - m_b_old);
//	paFloat a_new = m_g * (1 - m_b);
////	paFloat a_new = m_g * (1 - m_b_old);
//	paFloat aIncr = (a_new - a) / (paFloat)paBlock::nFrames;

	paFloat b = m_b_old;
	paFloat bIncr = (m_b - m_b_old) / (paFloat)paBlock::nFrames;

	paFloat g = m_g_old;
	paFloat gIncr = (m_g - m_g_old) / (paFloat)paBlock::nFrames;

// a = (1-b)g   :  interp g, b  however mult extra mult for a.

//! Alternative : seperate gain and filter, then 1 mult per samp for filter
//  Same mults overall, and can do linear interp for gain and filter cutoff, without extra mult.

	//! Interpolate m_b_old ?? Would reduce glitches when filter changes on low freq input.
	for(i=0; i < paBlock::nFrames; i++) {
//		y[i] = (1-b) * x[i] + b * m_y + (paFloat)DENORMALISATION_EPSILON;
		y[i] = x[i] + b * (m_y - x[i]) + (paFloat)DENORMALISATION_EPSILON;
//		y[i] = a * x[i] + b * m_y + (paFloat)DENORMALISATION_EPSILON;

		m_y = y[i];

		y[i] *= g;		// Gain is effectively a second stage.

		g += gIncr;
		b += bIncr;		//! Cutoff interp really does help sometimes.
	}

	m_b_old = m_b;
	m_g_old = m_g;





	return m_output;
}
