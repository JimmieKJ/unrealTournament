//
// paImpactGen.cpp
//
#include "PhyaPluginPrivatePCH.h"

#include "Impact/paImpactGen.hpp"



paImpactGen :: paImpactGen()
{
	m_poolHandle = -1;
	m_output = 0;
	m_relTangentSpeedAtImpact = 0;
	m_impactImpulse = 0;
	m_otherSurface = 0;
}


bool
paImpactGen :: isQuiet()
{
	return false;	//! Should be replaced with a measure of when impact has decayed.
};
