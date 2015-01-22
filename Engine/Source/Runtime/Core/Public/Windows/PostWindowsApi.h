// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

// Undo any Windows defines.
#undef uint8
#undef uint16
#undef uint32
#undef int32
#undef float
#undef MAX_uint8
#undef MAX_uint16
#undef MAX_uint32
#undef MAX_int32
#undef CDECL
#undef PF_MAX
#undef PlaySound
#undef DrawText
#undef CaptureStackBackTrace
#undef MemoryBarrier
#undef DeleteFile
#undef MoveFile
#undef CopyFile
#undef CreateDirectory

// Redefine CDECL to our version of the #define.  <AJS> Is this really necessary?
#define CDECL	    __cdecl					/* Standard C function */

// Notify people of the windows dependency. For CRITICAL_SECTION
#if !defined(_WINBASE_) && !defined(_XTL_)
	#error CRITICAL_SECTION relies on Windows.h/Xtl.h being included ahead of it
#endif

// Make sure version is high enough for API to be defined. For CRITICAL_SECTION
#if !defined(_XTL_) && (_WIN32_WINNT < 0x0403)
	#error SetCriticalSectionSpinCount requires _WIN32_WINNT >= 0x0403
#endif
