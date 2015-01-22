//
// paThread.cpp
//
#include "PhyaPluginPrivatePCH.h"


#include "System/paConfig.h"

#if _PA_THREADS

#include "System/paThread.hpp"
#include "Scene/paFlags.hpp"


paThreadHandle
paStartThread(unsigned (_stdcall *func)(void*)) {
	DWORD dwThreadID;
	HANDLE h;
//	HANDLE hprocess;
	BOOL result;
//	DWORD err;

//	hprocess = GetCurrentProcess();
//	err = GetLastError();
//	result = SetPriorityClass(hprocess, HIGH_PRIORITY_CLASS);
//	err = GetLastError();

//	err = GetLastError();
	h = (HANDLE)_beginthreadex(NULL, 0, func, NULL, 0, (unsigned*)&dwThreadID);
	paAssert(h != 0);
//	err = GetLastError();
//   THREAD_PRIORITY_TIME_CRITICAL   THREAD_PRIORITY_ABOVE_NORMAL
	result = SetThreadPriority(h, THREAD_PRIORITY_NORMAL);
	paAssert(result != 0);

	return h;
}




int
paStopThread( paThreadHandle h ) {

	paScene::audioThreadIsOn = false;		// Message picked up in audio thread in Generate.cpp
	// Should really wait until thread is dead, to prevent exception occuring if an external resource is deleted first.
	return 0;
}




bool
paThreadErr(paThreadHandle h) {
	//return(pid == -1);
	return (h == 0);
}


#endif // _PA_THREADS


