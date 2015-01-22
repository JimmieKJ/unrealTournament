//
// paObjectList.cpp
//
// Add / delete members from a list of pointers.
//
#include "PhyaPluginPrivatePCH.h"

#if 0
#include "System/paMemory.hpp"
#include "Utility/paObjectList.hpp"

template <class unit>
paObjectList<unit> :: paObjectList()
{
	m_nMaxMembers = 0;
}

template <class unit>
paObjectList<unit> :: ~paObjectList()
{
	paFree(m_list);
}


template <class unit>
int
paObjectList<unit> :: allocate(int n)
{
	if (m_nMaxMembers == 0 || n < 0 ) return -1;
	//error - allocation already made, or bad input.

	int r = m_listManager.allocate(n);
	//error - poolManager allocation fails
	if (r == -1) return -1;

	m_list = (unit**)paMalloc( m_nMaxMembers * sizeof(unit*) );
	//error - paMalloc failure

	m_nMaxMembers = n;
}

template <class unit>
int
paObjectList<unit> :: addMember(unit* member)
{
	int handle = (int)m_listManager.newHandle();
	if (handle == -1) return -1;
	//error - no objects left.

	m_list[handle] = member;

	return handle;
}


template <class unit>
void
paObjectList<unit> :: deleteMember(int h)
{
	m_listManager.deleteHandle( (paHandle)h );
}
#endif // 0
