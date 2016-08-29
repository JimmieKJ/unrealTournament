// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TickableObjectRenderThread.h"

// Base class for asynchronous loading splash.
class FAsyncLoadingSplash : public TSharedFromThis<FAsyncLoadingSplash>, public FGCObject
{
protected:
	class FTicker : public FTickableObjectRenderThread, public TSharedFromThis<FTicker>
	{
	public:
		FTicker(FAsyncLoadingSplash* InSplash) : FTickableObjectRenderThread(false, true), pSplash(InSplash) {}

		virtual void Tick(float DeltaTime) override { pSplash->Tick(DeltaTime); }
		virtual TStatId GetStatId() const override  { RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncLoadingSplash, STATGROUP_Tickables); }
		virtual bool IsTickable() const override	{ return pSplash->IsTickable(); }
	protected:
		FAsyncLoadingSplash* pSplash;
	};

public:
	USTRUCT()
	struct FSplashDesc
	{
		UPROPERTY()
		UTexture2D*			LoadingTexture;					// a UTexture pointer, either loaded manually or passed externally.
		FString				TexturePath;					// a path to a texture for auto loading, can be empty if LoadingTexture is specified explicitly
		FTransform			TransformInMeters;				// transform of center of quad (meters)
		FVector2D			QuadSizeInMeters;				// dimensions in meters
		FQuat				DeltaRotation;					// a delta rotation that will be added each rendering frame (half rate of full vsync)

		FSplashDesc() : LoadingTexture(nullptr)
			, TransformInMeters(FVector(4.0f, 0.f, 0.f))
			, QuadSizeInMeters(3.f, 3.f)
			, DeltaRotation(FQuat::Identity)
		{
		}
		bool operator==(const FSplashDesc& d) const
		{
			return LoadingTexture == d.LoadingTexture && TexturePath == d.TexturePath && 
				TransformInMeters.Equals(d.TransformInMeters) && 
				QuadSizeInMeters == d.QuadSizeInMeters && DeltaRotation.Equals(d.DeltaRotation);
		}
	};

	FAsyncLoadingSplash();
	virtual ~FAsyncLoadingSplash();

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FGCObject interface

	// FTickableObjectRenderThread implementation
	virtual void Tick(float DeltaTime) {}
	virtual bool IsTickable() const { return IsLoadingStarted() && !IsDone(); }
	// End of FTickableObjectRenderThread interface

	virtual void Startup();
	virtual void Shutdown();

	virtual bool IsLoadingStarted() const	{ return LoadingStarted; }
	virtual bool IsDone() const				{ return LoadingCompleted; }

	virtual void OnLoadingBegins();
	virtual void OnLoadingEnds();

	virtual bool AddSplash(const FSplashDesc&);
	virtual void ClearSplashes();
	virtual bool GetSplash(unsigned index, FSplashDesc& OutDesc);

	virtual void SetAutoShow(bool bInAuto) { bAutoShow = bInAuto; }
	virtual bool IsAutoShow() const { return bAutoShow; }

	virtual void SetLoadingIconMode(bool bInLoadingIconMode) { LoadingIconMode = bInLoadingIconMode; }
	virtual bool IsLoadingIconMode() const { return LoadingIconMode; }

	enum EShowType
	{
		None,
		ShowAtLoading,
		ShowManually
	};

	virtual void Show(enum EShowType) = 0;
	virtual void Hide(enum EShowType) = 0;

	// delegate method, called when loading begins
	void OnPreLoadMap(const FString&) { OnLoadingBegins(); }

	// delegate method, called when loading ends
	void OnPostLoadMap() { OnLoadingEnds(); }

protected:
	void LoadTexture(FSplashDesc& InSplashDesc);
	void UnloadTexture(FSplashDesc& InSplashDesc);

	TSharedPtr<FTicker>	RenTicker;

	const int32 SPLASH_MAX_NUM = 10;

	mutable FCriticalSection SplashScreensLock;
	UPROPERTY()
	TArray<FSplashDesc> SplashScreenDescs;

	FThreadSafeBool		LoadingCompleted;
	FThreadSafeBool		LoadingStarted;
	FThreadSafeBool		LoadingIconMode;		// this splash screen is a simple loading icon (if supported)
	bool				bAutoShow : 1;			// whether or not show splash screen automatically (when LoadMap is called)
	bool				bInitialized : 1;
};
