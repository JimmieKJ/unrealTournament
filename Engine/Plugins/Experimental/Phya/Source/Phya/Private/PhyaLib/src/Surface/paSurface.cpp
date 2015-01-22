//
// paSurface.cpp
//
#include "PhyaPluginPrivatePCH.h"


#include "Surface/paSurface.hpp"


paSurface ::paSurface()
{
	m_maxImpulse = -1.0f;	// No max limit.
	m_hardness = 1000.0f;	// Impulse width is about 1/1000 seconds.
	m_maxHardness = 0.0f;	// no max.
	m_impulseToHardnessBreakpoint = 0.0f;
	m_impulseToHardnessScale = 0.0f;

	m_contactAmpMax = (paFloat)-1.0;
	m_contactAmpMin = (paFloat)0.0;
	m_contactGain = (paFloat)1.0;
	m_contactCrossGain = (paFloat)1.0;
	m_contactDirectGain = (paFloat)0.0;

	m_impactAmpMax = (paFloat)-1.0;
	m_impactGain = (paFloat)1.0;
	m_impactCrossGain = (paFloat)1.0;
	m_impactDirectGain = (paFloat)0.0;

	m_contactDamping = (paFloat)1.0;
}
