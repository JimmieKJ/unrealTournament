//
// paImpact.cpp
//
// Describes the state of a continuous contact between two acoustic bodies.
//
#include "PhyaPluginPrivatePCH.h"

#include "System/paConfig.h"
#include "Impact/paImpact.hpp"
#include "Surface/paSurface.hpp"
#include "Resonator/paRes.hpp"
#include "Body/paBody.hpp"
#include "Scene/paLockCheck.hpp"


paObjectPool<paImpact> paImpact :: pool;


int
paImpact :: initialize()
{
	m_resonator1 = 0;
	m_resonator2 = 0;
	m_impactGen1 = 0;
	m_impactGen2 = 0;
	m_surface1 = 0;
	m_surface2 = 0;

	m_dynamicData.relTangentSpeedAtImpact = 0;
	m_dynamicData.impactImpulse = 0;

	m_isReady = false;

	return 0;
}

paImpact :: paImpact()
{
	m_poolHandle = -1;
	initialize();
}



int
paImpact :: terminate()
{
	if (m_surface1) m_surface1->deleteImpactGen(m_impactGen1);
	if (m_surface2) m_surface2->deleteImpactGen(m_impactGen2);
	return 0;
}



paImpact*
paImpact :: newImpact()
{
	paLOCKCHECK

	paImpact* i = paImpact::pool.newActiveObject();

	if (i == 0) return i;

	i->m_poolHandle = paImpact::pool.getHandle();

	i->initialize();

	return i;
}


int
paImpact :: deleteImpact(paImpact* i)
{
	paLOCKCHECK

	paAssert(i && "Impact not valid.");
//	if (i==0) return -1;

	paAssert(i->m_poolHandle != -1 && "Impact not in use.");
//	if (i->m_poolHandle == -1) return -1;	//error - contact not being used.

	i->terminate();
	paImpact::pool.deleteActiveObject(i->m_poolHandle);
	i->m_poolHandle = -1;
	return 0;
}




int
paImpact :: setBody1(paBody* b)
{
	paLOCKCHECK

	if (b == 0) { m_body1 = 0; return -1; }
	if (!b->isEnabled()) { m_body1 = 0; return -1; }

	paAssert(!m_impactGen1);

	m_body2 = b;
	m_resonator1 = b->getRes();
	m_surface1 = b->getSurface();

	if (!m_surface1) return -1;

	m_impactGen1 = m_surface1->newImpactGen();

	if (!m_impactGen1)
	{
		return -1;		// No point having an impact.
	};

	// Copy couplings from the body.

	setSurface1ImpactAmpMax(b->m_impactAmpMax);
	setSurface1ImpactGain(b->m_impactGain);
	setSurface1ImpactCrossGain(b->m_impactCrossGain);
	setSurface1ImpactDirectGain(b->m_impactDirectGain);

	// Let each gen know about the other gen's surface.
	// Used for calculating combined hardness.
	if (m_surface2)
	{
		m_impactGen2->setOtherSurface(m_surface1);
		m_impactGen1->setOtherSurface(m_surface2);
	}

	return 0;
}




int
paImpact :: setBody2(paBody* b)
{
	paLOCKCHECK

	if (b == 0) { m_body2 = 0; return -1; }
	if (!b->isEnabled()) { m_body2 = 0; return -1; }

	paAssert(!m_impactGen2);

	m_body2 = b;
	m_resonator2 = b->getRes();
	m_surface2 = b->getSurface();

	if (!m_surface2) return -1;

	m_impactGen2 = m_surface2->newImpactGen();

//	paAssert(m_impactGen2);
	if (!m_impactGen2) return -1;		// No point having a impact.

	// Copy couplings from the body.

	setSurface2ImpactAmpMax(b->m_impactAmpMax);
	setSurface2ImpactGain(b->m_impactGain);
	setSurface2ImpactCrossGain(b->m_impactCrossGain);
	setSurface2ImpactDirectGain(b->m_impactDirectGain);

	// Let each gen know about the other gen's surface.
	// Used for calculating combined hardness.
	if (m_surface1)
	{
		m_impactGen2->setOtherSurface(m_surface1);
		m_impactGen1->setOtherSurface(m_surface2);
	}

	return 0;
}


int
paImpact :: setDynamicData(paImpactDynamicData* d)
{
	paLOCKCHECK

	m_dynamicData = *d;
	m_isReady = true;
	return 0;
}

