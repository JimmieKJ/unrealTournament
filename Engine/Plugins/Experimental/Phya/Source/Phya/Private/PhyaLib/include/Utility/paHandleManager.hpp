//
// paHandleManager.hpp
//
// Purpose: To manage handles for a collection of objects. 
// The next available free handle is the most recently used one. 
// This means that any memory associated by the user with this handle is most likely
// to still be cached.
// firstHandle() and getNextHandle() are used to efficiently read through
// the list of active handles.
// While ennumerating the handles it is possible to delete the current handle, without
// upsetting the rest of the ennumeration.
//



#if !defined(__paHandleManager_hpp)
#define __paHandleManager_hpp

#include "System/paMemory.hpp"
#include "Utility/paHandleType.hpp"
#include "Utility/paRnd.hpp"

class PHYA_API paHandleManager
{
private:

	int m_nMaxHandles;
	int m_handleCounter;
	int m_nActiveHandles;			// A handle is active after it is selected with 'newHandle'.
									// Also points to the next free handle.
	int m_nMaxHandlesUsed;			// Records the greatest number of handles that have been in use
									// at one time. Used for guiding the user in future allocations.
	
	paHandle* m_handle;				// Array of handles. First m_nActiveHandles are 'in use'.
						
public:
	int* m_handle2index;			// This is the inverse of m_handle: m_handle2index[m_handle[i]]=i
	
	paHandleManager();
	~paHandleManager();

	int allocate(int n);
	void deallocate();

	paHandle newHandle();				// Return a currently unused handle.
	int deleteHandle(paHandle handle);	// Free handle for later re-use.
	void deleteAllHandles() { m_nActiveHandles = 0; };		// Note no need to swap elements in handle[].

	void firstHandle() { m_handleCounter = 0; }

	paHandle getNextHandle();

	int getnActiveHandles() { return m_nActiveHandles; };

	int getnMaxHandlesUsed() { return m_nMaxHandlesUsed; };

	bool isHandleActive(paHandle handle)
		{ return(m_handle2index[handle] != -1); };

	paHandle getRandomHandle();

};




#endif

