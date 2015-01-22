//
// paBody.cpp
//
#include "PhyaPluginPrivatePCH.h"

#include "Body/paBody.hpp"
#include "Surface/paSurface.hpp"

paBody ::	paBody()
{
	m_resonator = 0;
	m_surface = 0;
	m_isEnabled = true;

	m_contactAmpMax = (paFloat)-1.0;
	m_contactGain = (paFloat)1.0;
	m_contactCrossGain = (paFloat)1.0;
	m_contactDirectGain = (paFloat)0.0;

	m_impactAmpMax = (paFloat)-1.0;
	m_impactGain = (paFloat)1.0;
	m_impactCrossGain = (paFloat)1.0;
	m_impactDirectGain = (paFloat)0.0;
}



int
paBody ::	setSurface(paSurface* s)
{
	m_surface = s;

	m_maxImpulse = s->m_maxImpulse;

	// Copy couplings from surface, so that alterations
	// can be made for this body.

	m_contactGain = s->m_contactGain;
	m_contactAmpMax = s->m_contactAmpMax;
	m_contactCrossGain = s->m_contactCrossGain;
	m_contactDirectGain = s->m_contactDirectGain;

	m_impactGain = s->m_impactGain;
	m_impactAmpMax = s->m_impactAmpMax;
	m_impactCrossGain = s->m_impactCrossGain;
	m_impactDirectGain = s->m_impactDirectGain;

	return 0;
}


