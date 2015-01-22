//
// paCriticalSection.hpp
//



#if !defined(__paCriticalSection_hpp)
#define __paCriticalSection_hpp

#include "System/paConfig.h"

#if _PA_THREADS

#include <process.h>
#include <windows.h>


typedef CRITICAL_SECTION paCriticalSection;

inline void
paInitializeCriticalSection( paCriticalSection* section )
{
	InitializeCriticalSection( section );
}


inline void
paEnterCriticalSection( paCriticalSection* section )
{
	EnterCriticalSection( section );
}


inline void
paLeaveCriticalSection( paCriticalSection* section )
{
	LeaveCriticalSection( section );
}


inline void
paDeleteCriticalSection( paCriticalSection* section )
{
	DeleteCriticalSection( section );
}

#endif // _PA_THREADS


#endif
