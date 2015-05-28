// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CaptureSource.h: CaptureSource definition
=============================================================================*/

#ifndef _CAPTURESOURCE_HEADER_
#define _CAPTURESOURCE_HEADER_

#if PLATFORM_WINDOWS && !UE_BUILD_MINIMAL
#pragma warning(push)
#pragma warning(disable : 4263) // 'function' : member function does not override any base class virtual member function
#pragma warning(disable : 4264) // 'virtual_function' : no override available for virtual member function from base 
#include "AllowWindowsPlatformTypes.h"
#include <streams.h>
#include "HideWindowsPlatformTypes.h"
#pragma warning(pop)
class FCapturePin;

// {9A80E195-3BBA-4821-B18B-21BB496F80F8}
DEFINE_GUID(CLSID_CaptureSource, 
			0x9a80e195, 0x3bba, 0x4821, 0xb1, 0x8b, 0x21, 0xbb, 0x49, 0x6f, 0x80, 0xf8);

class FCaptureSource : public CSource
{

public:
	FCaptureSource(IUnknown *pUnk, HRESULT *phr);
	~FCaptureSource();
private:
	FCapturePin *CapturePin;

public:
	static CUnknown * WINAPI CreateInstance(IUnknown *pUnk, HRESULT *phr);  

};
#endif //#if PLATFORM_WINDOWS

#endif	//#ifndef _CAPTURESOURCE_HEADER_