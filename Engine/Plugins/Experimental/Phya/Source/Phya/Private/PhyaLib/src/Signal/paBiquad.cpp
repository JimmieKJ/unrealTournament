//
// paBiquad.cpp
//
#include "PhyaPluginPrivatePCH.h"


//#include <math.h>
#include "Signal/paBiquad.hpp"


paBiquad::paBiquad()
{
	m_input = 0;
	m_output = 0;

	// Initially no filtering.
	m_a0 = 1.0f;
	m_a1 = m_a2 = m_b1 = m_b2 = 0.0f;

	reset();
}

paBiquad::~paBiquad()
{
}


paBlock*
paBiquad::tick()
{
	paFloat* x = m_input->getStart();
	paFloat* y = m_output->getStart();

	int i;
	paFloat x0;

	for(i=0; i < paBlock::nFrames; i++) {

		x0 = x[i];
		y[i] = m_a0 * x0 + m_a1 * m_x1 + m_a2 * m_x2 
			    - m_b1 * m_y1 - m_b2 * m_y2  + (paFloat)DENORMALISATION_EPSILON;

		m_x2 = m_x1;
		m_x1 = x0;
		m_y2 = m_y1;
		m_y1 = y[i];

	}

	return m_output;
}
