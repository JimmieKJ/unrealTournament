//
// paRes.cpp
//
#include "PhyaPluginPrivatePCH.h"

#include "System/paConfig.h"

#include "Resonator/paRes.hpp"


paObjectList<paRes*>
paRes::activeResList;



paRes::paRes()
{
	m_activityHandle = -1;			// Not active initially.
	m_output = 0;
	m_makeQuiet = false;
	m_maxContactDamping = 100.0f;	// Initial limit chosen to prevent
									// coefficient calculation errors.
}


int
paRes::activate()
{
	zero();
	m_activityHandle = activeResList.addMember(this);
	paAssert(m_activityHandle != -1 && "Res activity list full.");
	return 0;
}



int
paRes::deactivate()
{
	activeResList.deleteMember(m_activityHandle);
	m_activityHandle = -1;
	return 0;
}


