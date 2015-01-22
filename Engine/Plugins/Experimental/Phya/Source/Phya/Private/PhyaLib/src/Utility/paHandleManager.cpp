//
// paHandleManager.cpp
//
#include "PhyaPluginPrivatePCH.h"


#include "System/paConfig.h"

#include "Utility/paHandleManager.hpp"
#include "Scene/paLockCheck.hpp"

paHandleManager :: paHandleManager()
{
	m_nMaxHandles = 0;
	m_nActiveHandles = 0;
	m_handleCounter = 0;
	m_nMaxHandlesUsed = 0;
}


paHandleManager :: ~paHandleManager()
{
	deallocate();
}


int
paHandleManager :: allocate(int nh)
{

	if (m_nMaxHandles != 0) return -1;
	//error already allocated.

	m_nMaxHandles = nh;
	m_nActiveHandles = 0;

	m_handle = (int*)paMalloc( nh * sizeof(int));
	m_handle2index = (int*)paMalloc( nh * sizeof(int));

	//error test for memory allocation failure.
	paAssert(m_handle != 0);
	paAssert(m_handle2index != 0);

	// Assign unique numeric handles for each index.
	// park free handles with '-1'
	int i;
	for(i=0; i<nh; i++)	{	
		m_handle[i] = i;
		m_handle2index[i] = -1;
	}

	return 0;
}


void
paHandleManager :: deallocate()
{
	paFree(m_handle);
	paFree(m_handle2index);
}



int
paHandleManager :: newHandle()
{
	//paAssert(!(m_nActiveHandles == m_nMaxHandles || m_nMaxHandles == 0));

	if (m_nActiveHandles == m_nMaxHandles || m_nMaxHandles == 0 ) return -1;

	int newInputHandle = m_handle[m_nActiveHandles];
	m_handle2index[newInputHandle] = m_nActiveHandles;


	m_nActiveHandles++;
	
	if (m_nActiveHandles > m_nMaxHandlesUsed) m_nMaxHandlesUsed = m_nActiveHandles;	// For record keeping.

	return newInputHandle;
}


int
paHandleManager :: deleteHandle(int deletedHandle)
{
	int deletedHandleIndex = m_handle2index[deletedHandle];


	paAssert(deletedHandleIndex != -1);
	if (deletedHandleIndex == -1) return -1;
	//error -handle was free already.

	m_nActiveHandles--;

	int lastHandleIndex = m_nActiveHandles;			// Biggest index of the active handles.
	int lastHandle = m_handle[lastHandleIndex];

	// Mark deleted handle as free - used to check that a handle is active before deleting.
	m_handle2index[deletedHandle] = -1;

	if (m_nActiveHandles == 0 
		|| lastHandle == deletedHandle) return 0;		// No moving required.

	// When ennumerating the handle list with getNextHandle, we may want to delete
	// the last handle read. The following line ensures that the rest of the list
	// can be ennumerated without a glitch, by back tracking to the slot where the
	// 'lastHandle' has moved to.
	if (deletedHandleIndex == m_handleCounter -1) m_handleCounter--;
	//error - should check that we havn't attempted to delete a handle previously
	// ennumerated while handles remain in the ennumeration.

	// Move the deleted handle to the new first free handle index.
	m_handle[m_nActiveHandles] = deletedHandle;		

	// Move the displaced 'lastHandle' to where the deleted handle was.
	m_handle[deletedHandleIndex] = lastHandle;
	m_handle2index[lastHandle] = deletedHandleIndex;

	return 0;
}


paHandle 
paHandleManager :: getNextHandle() { 
	if (m_handleCounter >= m_nActiveHandles) {
		m_handleCounter = -1;			// Indicates ennumeration finito.
		return -1;
	}
	return m_handle[m_handleCounter++]; 
};


paHandle
paHandleManager :: getRandomHandle() {
	paLOCKCHECK
	if (m_nActiveHandles > 0)	
		return m_handle[ paRnd((int)0, (int)m_nActiveHandles-1) ];

	return -1;
};

