// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HeadMountedDisplayPrivate.h"

/**
* HMD device console vars
*/
static TAutoConsoleVariable<int32> CVarHiddenAreaMask(
	TEXT("vr.HiddenAreaMask"),
	1,
	TEXT("0 to disable hidden area mask, 1 to enable."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

class FHeadMountedDisplayModule : public IHeadMountedDisplayModule
{
	virtual TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > CreateHeadMountedDisplay()
	{
		TSharedPtr<IHeadMountedDisplay, ESPMode::ThreadSafe> DummyVal = NULL;
		return DummyVal;
	}

	FString GetModulePriorityKeyName() const
	{
		return FString(TEXT("Default"));
	}
};

IMPLEMENT_MODULE( FHeadMountedDisplayModule, HeadMountedDisplay );

IHeadMountedDisplay::IHeadMountedDisplay()
{
	PreFullScreenRect = FSlateRect(-1.f, -1.f, -1.f, -1.f);
}

void IHeadMountedDisplay::PushPreFullScreenRect(const FSlateRect& InPreFullScreenRect)
{
	PreFullScreenRect = InPreFullScreenRect;
}

void IHeadMountedDisplay::PopPreFullScreenRect(FSlateRect& OutPreFullScreenRect)
{
	OutPreFullScreenRect = PreFullScreenRect;
	PreFullScreenRect = FSlateRect(-1.f, -1.f, -1.f, -1.f);
}