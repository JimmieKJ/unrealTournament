// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "OculusRiftPrivatePCH.h"
#include "OculusRiftHMD.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#include "OculusRiftSplash.h"

FOculusRiftSplash::FOculusRiftSplash(FOculusRiftHMD* InPlugin) : 
	LayerMgr(MakeShareable(new OculusRift::FLayerManager(InPlugin->GetCustomPresent_Internal())))
	, pPlugin(InPlugin)
	, DisplayRefreshRate(1/90.f)
	, ShowType(None)
{
	const TCHAR* SplashSettings = TEXT("Oculus.Splash.Settings");
	float f;
	FVector vec;
	FVector2D vec2d;
	FString s;
	bool b;
	FRotator r;
	if (GConfig->GetBool(SplashSettings, TEXT("bAutoEnabled"), b, GEngineIni))
	{
		bAutoShow = b;
	}
	FString num;
	for (int32 i = 0; ; ++i)
	{
		FSplashDesc SplashDesc;
		if (GConfig->GetString(SplashSettings, *(FString(TEXT("TexturePath")) + num), s, GEngineIni))
		{
			SplashDesc.TexturePath = s;
		}
		else
		{
			break;
		}
		if (GConfig->GetVector(SplashSettings, *(FString(TEXT("DistanceInMeters")) + num), vec, GEngineIni))
		{
			SplashDesc.TransformInMeters.SetTranslation(vec);
		}
		if (GConfig->GetRotator(SplashSettings, *(FString(TEXT("Rotation")) + num), r, GEngineIni))
		{
			SplashDesc.TransformInMeters.SetRotation(FQuat(r));
		}
		if (GConfig->GetVector2D(SplashSettings, *(FString(TEXT("SizeInMeters")) + num), vec2d, GEngineIni))
		{
			SplashDesc.QuadSizeInMeters = vec2d;
		}
		if (GConfig->GetRotator(SplashSettings, *(FString(TEXT("DeltaRotation")) + num), r, GEngineIni))
		{
			SplashDesc.DeltaRotation = FQuat(r);
		}
		else
		{
			if (GConfig->GetVector(SplashSettings, *(FString(TEXT("RotationAxis")) + num), vec, GEngineIni))
			{
				if (GConfig->GetFloat(SplashSettings, *(FString(TEXT("RotationDeltaInDegrees")) + num), f, GEngineIni))
				{
					SplashDesc.DeltaRotation = FQuat(vec, FMath::DegreesToRadians(f));
				}
			}
		}
		if (!SplashDesc.TexturePath.IsEmpty())
		{
			AddSplash(SplashDesc);
		}
		num = LexicalConversion::ToString(i);
	}
}

void FOculusRiftSplash::Startup()
{
	FAsyncLoadingSplash::Startup();

	ovrHmdDesc Desc = ovr_GetHmdDesc(nullptr);
	DisplayRefreshRate = 1.f / Desc.DisplayRefreshRate;
	LayerMgr->Startup();
}

void FOculusRiftSplash::Shutdown()
{
	SplashIsShown = false;
	UnloadTextures();
	LayerMgr->RemoveAllLayers();

	FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
	if (pCustomPresent)
	{
		pCustomPresent->UnlockSubmitFrame();
	}
	ShowingBlack = false;
	LayerMgr->Shutdown();

	FAsyncLoadingSplash::Shutdown();
}

void FOculusRiftSplash::PreShutdown()
{
	// force Ticks to stop
	SplashIsShown = false;
}

void FOculusRiftSplash::Tick(float DeltaTime)
{
	check(IsInRenderingThread());
	FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
	FGameFrame* pCurrentFrame = (FGameFrame*)RenderFrame.Get();
	if (pCustomPresent && pCurrentFrame && pCustomPresent->GetSession()->IsActive())
	{
		static double LastHighFreqTime = FPlatformTime::Seconds();
		const double CurTime = FPlatformTime::Seconds();
		const double DeltaSecondsHighFreq = CurTime - LastHighFreqTime;

		bool bNeedToPushFrame = false;
		FScopeLock ScopeLock2(&RenderSplashScreensLock);
		for (int32 i = 0; i < RenderSplashScreens.Num(); ++i)
		{
			FRenderSplashInfo& Splash = RenderSplashScreens[i];
			// Let update only each 1/3 secs or each 2nd frame if rotation is needed
			if (!Splash.Desc.DeltaRotation.Equals(FQuat::Identity) && DeltaSecondsHighFreq > 2.f * DisplayRefreshRate)
			{
				const FHMDLayerDesc* pLayerDesc = LayerMgr->GetLayerDesc(Splash.SplashLID);
				if (pLayerDesc)
				{
					FHMDLayerDesc layerDesc = *pLayerDesc;
					FTransform transform(layerDesc.GetTransform());
					if (!Splash.Desc.DeltaRotation.Equals(FQuat::Identity))
					{
						const FQuat prevRotation(transform.GetRotation());
						const FQuat resultRotation(Splash.Desc.DeltaRotation * prevRotation);
						transform.SetRotation(resultRotation);
						layerDesc.SetTransform(transform);
					}
					LayerMgr->UpdateLayer(layerDesc);
					bNeedToPushFrame = true;
				}
			}
		}
		if (bNeedToPushFrame || DeltaSecondsHighFreq > 1.f / 3.f)
		{
			FOvrSessionShared::AutoSession OvrSession(pCustomPresent->GetSession());
			pCurrentFrame->FrameNumber = pPlugin->GetCurrentFrameNumber();
			ovr_GetPredictedDisplayTime(OvrSession, pCurrentFrame->FrameNumber);

			LayerMgr->PreSubmitUpdate_RenderThread(FRHICommandListExecutor::GetImmediateCommandList(), pCurrentFrame, false);

			LayerMgr->SubmitFrame_RenderThread(OvrSession, pCurrentFrame, false);

			if (DeltaSecondsHighFreq > 0.5)
			{
				UE_LOG(LogHMD, Log, TEXT("FOculusRiftSplash::Tick, DELTA > 0.5 secs, ie: %.4f %.4f"), DeltaTime, float(DeltaSecondsHighFreq));
			}
			LastHighFreqTime = CurTime;
		}
	}
}

bool FOculusRiftSplash::IsTickable() const
{
	return /*(IsLoadingStarted() && !IsDone() && ShowingBlack) || */ SplashIsShown;
}

void FOculusRiftSplash::Show(EShowType InShowType)
{
	check(IsInGameThread());
	Hide(InShowType);

	ShowType = InShowType;

	FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
	if (pCustomPresent)
	{
		bool ReadyToPush = false;

		LayerMgr->RemoveAllLayers();

		// scope lock
		{
			FScopeLock ScopeLock(&SplashScreensLock);
			// Make sure all UTextures are loaded and contain Resource->TextureRHI
			bool bWaitForRT = false;
			for (int32 i = 0; i < SplashScreenDescs.Num(); ++i)
			{
				if (!SplashScreenDescs[i].TexturePath.IsEmpty())
				{
					// load temporary texture (if TexturePath was specified)
					LoadTexture(SplashScreenDescs[i]);
				}
				if (SplashScreenDescs[i].LoadingTexture->IsValidLowLevel())
				{
					SplashScreenDescs[i].LoadingTexture->UpdateResource();
					bWaitForRT = true;
				}
			}
			if (bWaitForRT)
			{
				FlushRenderingCommands();
			}
			for (int32 i = 0; i < SplashScreenDescs.Num(); ++i)
			{
				//@DBG BEGIN
				if (!SplashScreenDescs[i].LoadingTexture->IsValidLowLevel())
				{
					continue;
				}
				if (!SplashScreenDescs[i].LoadingTexture->Resource)
				{
					UE_LOG(LogHMD, Warning, TEXT("Splash, %s - no Resource"), *SplashScreenDescs[i].LoadingTexture->GetDesc());
				}
				else
				{
					UE_CLOG(!SplashScreenDescs[i].LoadingTexture->Resource->TextureRHI, LogHMD, Warning, TEXT("Splash, %s - no TextureRHI"), *SplashScreenDescs[i].LoadingTexture->GetDesc());
				}
				//@DBG END
				if (SplashScreenDescs[i].LoadingTexture->Resource && SplashScreenDescs[i].LoadingTexture->Resource->TextureRHI)
				{
					FRenderSplashInfo RenSplash;
					// use X (depth) as layers priority
					const uint32 Prio = FHMDLayerDesc::MaxPriority - uint32(SplashScreenDescs[i].TransformInMeters.GetTranslation().X * 1000.f);
					TSharedPtr<FHMDLayerDesc> layer = LayerMgr->AddLayer(FHMDLayerDesc::Quad, Prio, FHMDLayerManager::Layer_TorsoLocked, RenSplash.SplashLID);
					check(layer.IsValid());
					layer->SetTexture(SplashScreenDescs[i].LoadingTexture->Resource->TextureRHI);
					layer->SetTransform(SplashScreenDescs[i].TransformInMeters);
					layer->SetQuadSize(SplashScreenDescs[i].QuadSizeInMeters);

					ReadyToPush = true;

					RenSplash.Desc = SplashScreenDescs[i];

					FScopeLock ScopeLock2(&RenderSplashScreensLock);
					RenderSplashScreens.Add(RenSplash);
				}
			}
		}

		// this will push black frame, if texture is not loaded
		pPlugin->InitDevice();

		SplashIsShown = true;
		if (ReadyToPush)
		{
			ShowingBlack = false;
			PushFrame();
		}
		else
		{
			PushBlackFrame();
		}
		pCustomPresent->LockSubmitFrame();
		UE_LOG(LogHMD, Log, TEXT("FOculusRiftSplash::Show"));
	}
}

void FOculusRiftSplash::PushFrame()
{
	check(IsInGameThread());
	FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
	check(pCustomPresent);
	FOvrSessionShared::AutoSession OvrSession(pCustomPresent->GetSession());
	if (pCustomPresent && pCustomPresent->GetSession()->IsActive())
	{
		// Create a fake frame to pass it to layer manager
		TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> CurrentFrame = pPlugin->CreateNewGameFrame();
		CurrentFrame->Settings = pPlugin->GetSettings()->Clone();
		CurrentFrame->FrameNumber = pPlugin->GetCurrentFrameNumber(); // make sure no 0 frame is used.
		// keep units in meters rather than UU (because UU make not much sense).
		CurrentFrame->Settings->WorldToMetersScale = 1.0f;
		CurrentFrame->SetWorldToMetersScale( CurrentFrame->Settings->WorldToMetersScale );

		ovr_GetPredictedDisplayTime(OvrSession, CurrentFrame->FrameNumber);

		struct FSplashRenParams
		{
			FCustomPresent*		pCustomPresent;
			FHMDGameFramePtr	CurrentFrame;
			FHMDGameFramePtr*	RenderFramePtr;
		};

		FSplashRenParams params;
		params.pCustomPresent = pCustomPresent;
		params.CurrentFrame = CurrentFrame;
		params.RenderFramePtr = &RenderFrame;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(SubmitSplash,
		const FSplashRenParams&, Params, params,
		FLayerManager*, LayerMgr, LayerMgr.Get(),
		{
			*Params.RenderFramePtr = Params.CurrentFrame;

			auto pCurrentFrame = (FGameFrame*)Params.CurrentFrame.Get();
			FOvrSessionShared::AutoSession OvrSession(Params.pCustomPresent->GetSession());
			check(Params.pCustomPresent->GetSession()->IsActive());
			ovr_GetPredictedDisplayTime(OvrSession, pCurrentFrame->FrameNumber);
			LayerMgr->PreSubmitUpdate_RenderThread(RHICmdList, pCurrentFrame, false);
			LayerMgr->SubmitFrame_RenderThread(OvrSession, pCurrentFrame, false);
		});
		FlushRenderingCommands();
	}
}

void FOculusRiftSplash::PushBlackFrame()
{
	check(IsInGameThread());
	ShowingBlack = true;
	LayerMgr->RemoveAllLayers();

	// create an empty quad layer with no texture
	uint32 BlackSplashLID;
	TSharedPtr<FHMDLayerDesc> layer = LayerMgr->AddLayer(FHMDLayerDesc::Quad, 0, FHMDLayerManager::Layer_TorsoLocked, BlackSplashLID);
	check(layer.IsValid());
	layer->SetQuadSize(FVector2D(0.01f, 0.01f));
	PushFrame();
	UE_LOG(LogHMD, Log, TEXT("FOculusRiftSplash::PushBlackFrame"));
}

void FOculusRiftSplash::UnloadTextures()
{
	check(IsInGameThread());
	// unload temporary loaded textures
	FScopeLock ScopeLock(&SplashScreensLock);
	for (int32 i = 0; i < SplashScreenDescs.Num(); ++i)
	{
		FRenderSplashInfo RenSplash;
		if (!SplashScreenDescs[i].TexturePath.IsEmpty())
		{
			UnloadTexture(SplashScreenDescs[i]);
		}
	}
}

void FOculusRiftSplash::Hide(EShowType InShowType)
{
	check(IsInGameThread());
	if ((InShowType == ShowManually || InShowType == ShowType) && SplashIsShown)
	{
		UE_LOG(LogHMD, Log, TEXT("FOculusRiftSplash::Hide"));
		{
			FScopeLock ScopeLock2(&RenderSplashScreensLock);
			for (int32 i = 0; i < RenderSplashScreens.Num(); ++i)
			{
				LayerMgr->RemoveLayer(RenderSplashScreens[i].SplashLID);
			}
			RenderSplashScreens.SetNum(0);
		}
		UnloadTextures();

		PushBlackFrame();

		FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
		if (pCustomPresent)
		{
			pCustomPresent->UnlockSubmitFrame();
		}
		SplashIsShown = false;
		ShowType = None;
	}
}

#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS

