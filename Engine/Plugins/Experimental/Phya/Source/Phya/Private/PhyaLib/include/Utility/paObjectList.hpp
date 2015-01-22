//
// paObjectList.hpp
//
// Add / delete members from a list.
// The added members are copied directly to the preallocated memory.
//

#if !defined(__paObjectList_hpp)
#define __paObjectList_hpp



#ifdef MEAREPORT
#include <stdio.h>
#endif

#include "System/paConfig.h"
#include "System/paMemory.hpp"
#include "paHandleManager.hpp"

/*
    #ifdef EXP_STL
    #    define DECLSPECIFIER __declspec(dllexport)
    #    define EXPIMP_TEMPLATE
    #else
    #    define DECLSPECIFIER __declspec(dllimport)
    #    define EXPIMP_TEMPLATE extern
    #endif

    // Instantiate classes vector<int> and vector<char>
    // This does not create an object. It only forces the generation of all
    // of the members of classes vector<int> and vector<char>. It exports
    // them from the DLL and imports them into the .exe file.
    EXPIMP_TEMPLATE template class DECLSPECIFIER paObjectList<int>;
    EXPIMP_TEMPLATE template class DECLSPECIFIER std::vector<char>;


*/

template <class memberType>
class PHYA_API paObjectList
{
private:


	memberType* m_list;		// Array of member objects.
	int m_nMaxMembers;

public:
	paHandleManager m_listManager;


	void deallocate()
	{
		if (m_list == 0) return;
		paFree(m_list);
		m_list = 0;
	}


	paObjectList()
	{
		m_list = 0;
		m_nMaxMembers = 0;
	}

	~paObjectList()
	{
		deallocate();
	}


	int allocate(int n)
	{
		if (m_nMaxMembers >0 || n <= 0) return -1;
		paAssert(!(m_nMaxMembers >0 || n <= 0));
		//error - allocation already made, or bad input.

		int r = m_listManager.allocate(n);
		//error - poolManager allocation fails
		paAssert(r != -1);
		if (r == -1) return -1;

		m_list = (memberType*)paMalloc(n * sizeof(memberType));
		//error - paMalloc failure
		paAssert(m_list != 0);


		m_nMaxMembers = n;
		return 0;
	}

	int addMember(memberType member)
	{
		int handle = (int)m_listManager.newHandle();

		if (handle == -1) return -1;
		//error - no objects left.

		m_list[handle] = member;

#ifdef MEAREPORT2
		printf("paObjectList : handle %d\n", handle);
		printf("paObjectList : &m_list[handle] %d\n", &m_list[handle]);
		printf("paObjectList : &m_listManager.m_handle2index[0] %d\n",&m_listManager.m_handle2index[0]);
		printf("paObjectList : m_listManager.m_handle2index[0] %d\n",
		m_listManager.m_handle2index[0]);
#endif
		return handle;
	}


	void deleteMember(int h)
	{
		m_listManager.deleteHandle( (paHandle)h );
	}

	bool isMember( int handle ) 
	{ 
		return m_listManager.isHandleActive(handle); 
	}
	
	void firstMember() 
	{ 
		m_listManager.firstHandle(); 
	}
	
	memberType getNextMember() 
	{
		int h = m_listManager.getNextHandle();
		if (h == -1) return 0;
		else return m_list[h];
	}

	int getnMembers() 
	{ 
		return m_listManager.getnActiveHandles(); 
	}
	
	int getnMaxAllocationsUsed() 
	{ 
		return m_listManager.getnMaxHandlesUsed(); 
	};

	memberType getRandomMember() 
	{
		return m_list[m_listManager.getRandomHandle()]; 
	}
};

#endif
