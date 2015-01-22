// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	WinRTString.h: WinRT platform string classes, mostly implemented with ANSI C++
==============================================================================================*/

#pragma once
#include "GenericPlatform/MicrosoftPlatformString.h"
#include "HAL/Platform.h"

/**
 * WinRT string implementation
 */
struct FWinRTString : public FMicrosoftPlatformString
{
	// These should be replaced with equivalent Convert and ConvertedLength functions when we properly support variable-length encodings.
/*	static void WideCharToMultiByte(const wchar_t *Source, uint32 LengthWM1, ANSICHAR *Dest, uint32 LengthA)
	{
		::WideCharToMultiByte(CP_ACP,0,Source,LengthWM1+1,Dest,LengthA,NULL,NULL);
	}

	static void MultiByteToWideChar(const ANSICHAR *Source, TCHAR *Dest, uint32 LengthM1)
	{
		::MultiByteToWideChar(CP_ACP,0,Source,LengthM1+1,Dest,LengthM1+1);
	}

	static uint32 StrlenWide(const wchar_t* Source)
	{
		return wcslen(Source);
	}

	static uint32 StrlenAnsi(const ANSICHAR* Source)
	{
		return strlen(Source);
	}*/

	CORE_API static int32 GetVarArgs( WIDECHAR* Dest, SIZE_T DestSize, int32 Count, const WIDECHAR*& Fmt, va_list ArgPtr );
	CORE_API static int32 GetVarArgs( ANSICHAR* Dest, SIZE_T DestSize, int32 Count, const ANSICHAR*& Fmt, va_list ArgPtr );
};

typedef FWinRTString FPlatformString;
