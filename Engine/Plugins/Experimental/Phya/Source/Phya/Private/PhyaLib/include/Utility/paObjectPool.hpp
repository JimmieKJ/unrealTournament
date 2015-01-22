//
// paObjectPool.hpp
//
// Templated class for predefining and managing a resource pool
// of objects. Objects are instantiated on allocation.
//


#if !defined(__paObjectPool_hpp)
#define __paObjectPool_hpp



#include "System/paMemory.hpp"
#include "paHandleManager.hpp"

template <class unit>
class PHYA_API paObjectPool
{
private:

	paHandleManager m_poolManager;

	unit** m_pool;				// Array of pointers to pool objects.
	int m_nObjects;
	int m_handle;

public:


	paObjectPool()
	{
		m_nObjects = 0;
	}

	void reset()
	{
		firstActiveObject();
		paHandle handle;

		while( (handle = m_poolManager.getNextHandle()) != -1 )
		{
			m_poolManager.deleteHandle(handle); 
		}
	}

	void deallocate()
	{
		int i;

		if (m_nObjects == 0) return;

		for(i=0; i< m_nObjects; i++)
			if (m_pool[i] != 0) delete m_pool[i];

		paFree(m_pool);
		m_nObjects = 0;
	}

	~paObjectPool()
	{
		deallocate();
	}

	int allocate(int n)
	{
		if (m_nObjects != 0 || n <= 0 ) return -1;
		//error - allocation already made, or bad input.

		int r = m_poolManager.allocate(n);
		//error - poolManager allocation fails
		if (r == -1) return -1;

		m_pool = (unit**)paMalloc( n * sizeof(unit*) );
		//error - paMalloc failure

		int i;
		for(i=0; i< n; i++)
			m_pool[i] = new unit;
		//error - new failure

		m_nObjects = n;
		return 0;
	}

	unit* newActiveObject()
	{
		m_handle = (int)m_poolManager.newHandle();

#ifdef POOL_ASSERT
	paAssert(m_handle != -1  && "Pool object request failed.");
#endif
		if (m_handle == -1) return 0;

		return m_pool[(int)m_handle];
	}


	unit* getRandomObject() 
	{
		int h = m_poolManager.getRandomHandle();
		if (h != -1)
			return m_pool[h];
		else 
			return 0;
	}

	inline int getHandle() 
	{ 
		return m_handle; 
	}	// Should be stored after a newObject(), for later deleteObject();
	
	inline int deleteActiveObject(int h) 
	{ 
		return m_poolManager.deleteHandle((paHandle)h); 
	}

	inline void firstActiveObject() 
	{ 
		m_poolManager.firstHandle(); 
	}

	inline unit* getNextActiveObject() 
	{
		int h = m_poolManager.getNextHandle();
		if (h == -1) return 0;
		else return m_pool[h];
	}

	inline bool isObjectActive(int h) 
	{ 
		return m_poolManager.isHandleActive(h); 
	}

	inline int getnMaxAllocationsUsed() 
	{ 
		return m_poolManager.getnMaxHandlesUsed(); 
	}

	inline int getnActiveObjects() 
	{ 
		return m_poolManager.getnActiveHandles(); 
	}

	inline int getnObjects() 
	{ 
		return m_nObjects; 
	}
};


#endif

