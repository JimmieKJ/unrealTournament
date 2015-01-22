//
// paHighpass.cpp
//
#include "PhyaPluginPrivatePCH.h"


#include "Signal/paHighpass.hpp"


paHighpass::paHighpass()
{
	m_input = 0;
	m_output = 0;
}

paHighpass::~paHighpass()
{
}


paBlock*
paHighpass::tick()
{
	m_lowpass.tick(m_input, &m_temp);	// Could avoid m_temp if highpass is calculated in one step (not using paLowpass)

//	m_output->copy(m_input);
//	m_output->addWithMultiply(&m_temp, -1.0);

	return m_output;
}
