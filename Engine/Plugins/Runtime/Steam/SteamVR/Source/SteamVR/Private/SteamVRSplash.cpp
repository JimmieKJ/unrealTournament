// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SteamVRSplash.h"
#include "SteamVRPrivate.h"
#include "SteamVRHMD.h"

#if STEAMVR_SUPPORTED_PLATFORMS

FSteamSplashTicker::FSteamSplashTicker(class FSteamVRHMD* InSteamVRHMD)
	: FTickableObjectRenderThread(false, true)
	, SteamVRHMD(InSteamVRHMD)
{}

void FSteamSplashTicker::RegisterForMapLoad()
{
	FCoreUObjectDelegates::PreLoadMap.AddSP(this, &FSteamSplashTicker::OnPreLoadMap);
	FCoreUObjectDelegates::PostLoadMap.AddSP(this, &FSteamSplashTicker::OnPostLoadMap);
}

void FSteamSplashTicker::UnregisterForMapLoad()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMap.RemoveAll(this);
};

void FSteamSplashTicker::OnPreLoadMap(const FString&)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(RegisterAsyncTick,
		FTickableObjectRenderThread*, Ticker, this,
		{
			Ticker->Register();
		});
}

void FSteamSplashTicker::OnPostLoadMap()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(UnregisterAsyncTick, 
		FTickableObjectRenderThread*, Ticker, this,
		{
			Ticker->Unregister();
		});
}

void FSteamSplashTicker::Tick(float DeltaTime)
{
#if PLATFORM_WINDOWS
	if (SteamVRHMD->pD3D11Bridge && SteamVRHMD->VRCompositor && SteamVRHMD->bSplashIsShown)
	{
		SteamVRHMD->pD3D11Bridge->FinishRendering();
		SteamVRHMD->VRCompositor->PostPresentHandoff();
	}
#endif
}
TStatId FSteamSplashTicker::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FSplashTicker, STATGROUP_Tickables);
}

bool FSteamSplashTicker::IsTickable() const
{
	return true;
}

#endif
