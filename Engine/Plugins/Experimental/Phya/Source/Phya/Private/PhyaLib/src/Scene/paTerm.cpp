// paTerm.cpp
//
// Currently not needed, but could be used in the future.

#include "PhyaPluginPrivatePCH.h"

#include "System/paConfig.h"

#if USING_PA_TERM
#include "SignalProcess/paBlock.hpp"
#include "Resonator/paRes.hpp"
#include "Contact/paContact.hpp"
#include "Impact/paImpact.hpp"



extern"C"{

void             paLock();
void             paUnlock();

void
paTerm()
{

// Use for checking termination of statically created objects.
/*
	paLock();
		paBlock::pool.deallocate();
		paSegSurface::contactGenPool.deallocate();
		paSegSurface::impactGenPool.deallocate();
		paContact::pool.deallocate();
		paImpact::pool.deallocate();
		paRes::activeResList.deallocate();
	paUnlock();
*/
}

}	// extern"C"

#endif // USING_PA_TERM

