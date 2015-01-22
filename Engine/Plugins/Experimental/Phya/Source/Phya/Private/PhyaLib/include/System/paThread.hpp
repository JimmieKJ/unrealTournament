//
// paThread.hpp
//

#ifndef _paThread_hpp
#define _paThread_hpp

#include "System/paConfig.h"

#if _PA_THREADS

#define PHYA_THREAD_RETURN_TYPE  unsigned _stdcall

#include <process.h>
#include <windows.h>
#include "System/paConfig.h"

typedef HANDLE paThreadHandle;

PHYA_API
paThreadHandle
paStartThread(unsigned (_stdcall *func)(void*));

PHYA_API int paStopThread( paThreadHandle t );

PHYA_API bool paThreadErr(paThreadHandle h);

#endif // _PA_THREADS

#endif
