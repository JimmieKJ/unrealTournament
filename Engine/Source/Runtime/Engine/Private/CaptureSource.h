// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CaptureSource.h: CaptureSource definition
=============================================================================*/

#pragma once

#if PLATFORM_WINDOWS && !UE_BUILD_MINIMAL
#pragma warning(push)
#pragma warning(disable : 4263) // 'function' : member function does not override any base class virtual member function
#pragma warning(disable : 4264) // 'virtual_function' : no override available for virtual member function from base 
#include "AllowWindowsPlatformTypes.h"
#include <streams.h>
#include "HideWindowsPlatformTypes.h"
#pragma warning(pop)
class FCapturePin;
class FAVIWriter;

// {F817F8A7-DE00-42CF-826A-7A5654602D8E}
DEFINE_GUID(CLSID_ViewportCaptureSource, 
	0xf817f8a7, 0xde00, 0x42cf, 0x82, 0x6a, 0x7a, 0x56, 0x54, 0x60, 0x2d, 0x8e);


class FCaptureSource : public CSource
{
public:
	FCaptureSource(const FAVIWriter& Writer);
	~FCaptureSource();

	void StopCapturing();
	void OnFinishedCapturing();
	bool ShouldCapture() const { return !bShutdownRequested; }

private:
	FEvent* ShutdownEvent;
	FThreadSafeBool bShutdownRequested;
};
#endif //#if PLATFORM_WINDOWS

