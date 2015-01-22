// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostWinRTApi.h: The post-include part of UE4's WinRT API wrapper.
=============================================================================*/

// Undo any WinRT defines.
#undef BYTE
#undef WORD
#undef DWORD
#undef INT
#undef FLOAT
#undef MAXBYTE
#undef MAXWORD
#undef MAXDWORD
#undef MAXINT
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

// Redefine CDECL to our version of the #define.
#define CDECL	    __cdecl					/* Standard C function */
// Redefine CDECL to our version of the #define.
#define CDECL	    __cdecl					/* Standard C function */
