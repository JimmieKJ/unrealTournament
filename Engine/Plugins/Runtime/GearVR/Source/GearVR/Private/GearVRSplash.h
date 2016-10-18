// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IHeadMountedDisplay.h"

#if GEARVR_SUPPORTED_PLATFORMS

#include "HeadMountedDisplayCommon.h"
#include "AsyncLoadingSplash.h"

// Implementation of async splash for GearVR
class FGearVRSplash : public FAsyncLoadingSplash
{
public:
	FGearVRSplash(class FGearVR*);

	virtual void Tick(float DeltaTime);
	virtual bool IsTickable() const override;

	virtual void Startup() override;
	virtual void Shutdown() override;

	virtual void Show(enum EShowType) override;
	virtual void Hide(enum EShowType) override;

	virtual void SetLoadingIconMode(bool bInLoadingIconMode);

	void PreShutdown();

protected:

	void PushFrame();
	void PushBlackFrame();
	void UnloadTextures();

private:
	class FGearVR*		pPlugin;
	TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> RenderFrame;
	TSharedPtr<FHMDLayerManager> LayerMgr;

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

#endif //GEARVR_SUPPORTED_PLATFORMS
