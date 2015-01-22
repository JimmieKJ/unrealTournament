// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HideWinRTPlatformTypes.h: Defines for hiding WinRT type names.
=============================================================================*/

#ifdef WINRT_PLATFORM_TYPES_GUARD
	#undef WINRT_PLATFORM_TYPES_GUARD
#else
	#error Mismatched HideWinRTPLatformTypes.h detected.
#endif

#undef INT
#undef UINT
#undef DWORD
#undef FLOAT

