// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOculusRiftPlugin.h"
#include "IHeadMountedDisplay.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#include "HeadMountedDisplayCommon.h"
#include "AsyncLoadingSplash.h"
#include "OculusRiftLayers.h"

// Implementation of async splash for OculusRift
class FOculusRiftSplash : public FAsyncLoadingSplash
{
public:
	FOculusRiftSplash(class FOculusRiftHMD*);

	virtual void Tick(float DeltaTime);
	virtual bool IsTickable() const override;

	virtual void Startup() override;
	virtual void Shutdown() override;

	virtual void Show(enum EShowType) override;
	virtual void Hide(enum EShowType) override;


	void PreShutdown();

protected:

	void PushFrame();
	void PushBlackFrame();
	void UnloadTextures();

private:
	class FOculusRiftHMD*		pPlugin;
	TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> RenderFrame;
	TSharedPtr<OculusRift::FLayerManager> LayerMgr;

	struct FRenderSplashInfo
	{
		FSplashDesc Desc;
		uint32	SplashLID;

		FRenderSplashInfo() :SplashLID(~0u) {}	
	};
	mutable FCriticalSection	RenderSplashScreensLock;
	TArray<FRenderSplashInfo>	RenderSplashScreens;

	FThreadSafeBool				ShowingBlack;
	FThreadSafeBool				SplashIsShown;
	float						DisplayRefreshRate;
	EShowType					ShowType;
};

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
