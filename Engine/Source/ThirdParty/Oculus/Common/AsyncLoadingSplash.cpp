// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AsyncLoadingSplash.h"

DEFINE_LOG_CATEGORY_STATIC(LogLoadingSplash, Log, All);

FAsyncLoadingSplash::FAsyncLoadingSplash() : 
	bAutoShow(true)
	, bInitialized(false)
{
}

FAsyncLoadingSplash::~FAsyncLoadingSplash()
{
	// Make sure RenTicker is freed in Shutdown
	check(!RenTicker.IsValid())
}

void FAsyncLoadingSplash::AddReferencedObjects(FReferenceCollector& Collector)
{
	FScopeLock ScopeLock(&SplashScreensLock);
	for (int32 i = 0; i < SplashScreenDescs.Num(); ++i)
	{
		if (SplashScreenDescs[i].LoadingTexture)
		{
			Collector.AddReferencedObject(SplashScreenDescs[i].LoadingTexture);
		}
	}
}

void FAsyncLoadingSplash::Startup()
{
	if (!bInitialized)
	{
		RenTicker = MakeShareable(new FTicker(this));
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(RegisterAsyncTick,
			FTickableObjectRenderThread*, RenTicker, RenTicker.Get(),
			{
				RenTicker->Register();
			});

		// Add a delegate to start playing movies when we start loading a map
		FCoreUObjectDelegates::PreLoadMap.AddSP(this, &FAsyncLoadingSplash::OnPreLoadMap);
		FCoreUObjectDelegates::PostLoadMap.AddSP(this, &FAsyncLoadingSplash::OnPostLoadMap);
		bInitialized = true;
	}
}

void FAsyncLoadingSplash::Shutdown()
{
	if (bInitialized)
	{
		{
			FScopeLock ScopeLock(&SplashScreensLock);
			for (int32 i = 0; i < SplashScreenDescs.Num(); ++i)
			{
				UnloadTexture(SplashScreenDescs[i]);
			}
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(UnregisterAsyncTick, 
		TSharedPtr<FTicker>&, RenTicker, RenTicker,
		{
			RenTicker->Unregister();
			RenTicker = nullptr;
		});
		FlushRenderingCommands();

		FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
		FCoreUObjectDelegates::PostLoadMap.RemoveAll(this);

		bInitialized = false;
		LoadingCompleted = false;
		LoadingStarted = false;
	}
}

void FAsyncLoadingSplash::LoadTexture(FSplashDesc& InSplashDesc)
{
	check(IsInGameThread());
	UnloadTexture(InSplashDesc);
	
	UE_LOG(LogLoadingSplash, Log, TEXT("Loading texture for splash %s..."), *InSplashDesc.TexturePath);
	InSplashDesc.LoadingTexture = LoadObject<UTexture2D>(NULL, *InSplashDesc.TexturePath, NULL, LOAD_None, NULL);
	if (InSplashDesc.LoadingTexture != nullptr)
	{
		UE_LOG(LogLoadingSplash, Log, TEXT("...Success. "));
	}
	InSplashDesc.LoadedTexture = nullptr;
}

void FAsyncLoadingSplash::UnloadTexture(FSplashDesc& InSplashDesc)
{
	check(IsInGameThread());
	if (InSplashDesc.LoadingTexture && InSplashDesc.LoadingTexture->IsValidLowLevel())
	{
		InSplashDesc.LoadingTexture = nullptr;
	}
	InSplashDesc.LoadedTexture = nullptr;
}

void FAsyncLoadingSplash::OnLoadingBegins()
{
	if (bAutoShow)
	{
		UE_LOG(LogLoadingSplash, Log, TEXT("Loading begins"));
		LoadingStarted = true;
		LoadingCompleted = false;
		Show(ShowAtLoading);
	}
}

void FAsyncLoadingSplash::OnLoadingEnds()
{
	if (bAutoShow)
	{
		UE_LOG(LogLoadingSplash, Log, TEXT("Loading ends"));
		LoadingStarted = false;
		LoadingCompleted = true;
		Hide(ShowAtLoading);
	}
}

bool FAsyncLoadingSplash::AddSplash(const FSplashDesc& Desc)
{
	check(IsInGameThread());
	FScopeLock ScopeLock(&SplashScreensLock);
	if (SplashScreenDescs.Num() < SPLASH_MAX_NUM)
	{
#if !UE_BUILD_SHIPPING
		// check if we already have very same layer; if yes, print out a warning
		for (int32 i = 0; i < SplashScreenDescs.Num(); ++i)
		{
			if (SplashScreenDescs[i] == Desc)
			{
				UE_LOG(LogHMD, Warning, TEXT(""))
			}
		}
#endif // #if !UE_BUILD_SHIPPING
		SplashScreenDescs.Add(Desc);
		return true;
	}
	return false;
}

void FAsyncLoadingSplash::ClearSplashes()
{
	check(IsInGameThread());
	FScopeLock ScopeLock(&SplashScreensLock);
	for (int32 i = 0; i < SplashScreenDescs.Num(); ++i)
	{
		UnloadTexture(SplashScreenDescs[i]);
	}
	SplashScreenDescs.SetNum(0);
}

bool FAsyncLoadingSplash::GetSplash(unsigned index, FSplashDesc& OutDesc)
{
	check(IsInGameThread());
	FScopeLock ScopeLock(&SplashScreensLock);
	if (index < unsigned(SplashScreenDescs.Num()))
	{
		OutDesc = SplashScreenDescs[int32(index)];
		return true;
	}
	return false;
}
