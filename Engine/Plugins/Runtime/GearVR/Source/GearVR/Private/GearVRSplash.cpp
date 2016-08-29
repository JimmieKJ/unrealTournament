// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "GearVRPrivatePCH.h"
#include "GearVR.h"

#if GEARVR_SUPPORTED_PLATFORMS

#include "GearVRSplash.h"

typedef FCustomPresent FCustomPresent; // TEMP

FGearVRSplash::FGearVRSplash(FGearVR* InPlugin) : 
	LayerMgr(MakeShareable(new GearVR::FLayerManager(InPlugin->GetCustomPresent_Internal())))
	, pPlugin(InPlugin)
	, DisplayRefreshRate(1.f / 60.f)
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

	int RequestedLoadingIconMode = 0; // 0 -default, -1 - false, 1 - true
	if (GConfig->GetBool(SplashSettings, TEXT("bLoadingIcon"), b, GEngineIni))
	{
		RequestedLoadingIconMode = (b) ? 1 : -1;
	}

	FString num;
	bool bAtLeastOneTextureSpecified = false;
	for (int32 i = 0;; ++i)
	{
		FSplashDesc SplashDesc;
		if (GConfig->GetString(SplashSettings, *(FString(TEXT("TexturePath")) + num), s, GEngineIni))
		{
			bAtLeastOneTextureSpecified = true;
			SplashDesc.TexturePath = s;

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
		}
		if (!SplashDesc.TexturePath.IsEmpty())
		{
			AddSplash(SplashDesc);
		}
		else
		{
			break;
		}
		num = LexicalConversion::ToString(i);
	}

	// set loading icon mode in the case if:
	// 1) it is explicitly set to true of false
	// 2) it is not set explicitly and there is no texture specified (the default loading icon will be used)
	SetLoadingIconMode(RequestedLoadingIconMode > 0 || (RequestedLoadingIconMode == 0 && !bAtLeastOneTextureSpecified));
}

void FGearVRSplash::Startup()
{
	FAsyncLoadingSplash::Startup();

	LayerMgr->Startup();
}

void FGearVRSplash::Shutdown()
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

void FGearVRSplash::PreShutdown()
{
	// force Ticks to stop
	SplashIsShown = false;
}

void FGearVRSplash::Tick(float DeltaTime)
{
	check(IsInRenderingThread());
	FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
	if (pCustomPresent && pPlugin->HasValidOvrMobile())
	{
		static double LastHighFreqTime = FPlatformTime::Seconds();
		const double CurTime = FPlatformTime::Seconds();
		const double DeltaSecondsHighFreq = CurTime - LastHighFreqTime;

		if (!IsLoadingIconMode())
		{
			bool bNeedToPushFrame = false;
			FScopeLock ScopeLock2(&RenderSplashScreensLock);
			FGameFrame* pCurrentFrame = static_cast<FGameFrame*>(RenderFrame.Get());
			check(pCurrentFrame);
			for (int32 i = 0; i < RenderSplashScreens.Num(); ++i)
			{
				FRenderSplashInfo& Splash = RenderSplashScreens[i];
				// Let update only each 2nd frame if rotation is needed
				if ((!Splash.Desc.DeltaRotation.Equals(FQuat::Identity) && DeltaSecondsHighFreq > 2.f * DisplayRefreshRate))
				{
					if (pCurrentFrame)
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
			}
			if (bNeedToPushFrame || DeltaSecondsHighFreq > .5f) // also call push frame once in half second
			{
				++pCurrentFrame->FrameNumber; // = pPlugin->GetCurrentFrameNumber();

				FLayerManager* pGearVRLayerMgr = (FLayerManager*)LayerMgr.Get();
				pGearVRLayerMgr->PreSubmitUpdate_RenderThread(FRHICommandListExecutor::GetImmediateCommandList(), pCurrentFrame, false);

				pCustomPresent->PushFrame(pGearVRLayerMgr, pCurrentFrame);
				LastHighFreqTime = CurTime;
			}
		}
		else if (DeltaSecondsHighFreq > 1.f / 3.f)
		{
			pPlugin->RenderLoadingIcon_RenderThread();
			LastHighFreqTime = CurTime;
		}
	}
}

bool FGearVRSplash::IsTickable() const
{
	return SplashIsShown && !ShowingBlack;
}

void FGearVRSplash::Show(EShowType InShowType)
{
	check(IsInGameThread());
	Hide(InShowType);

	ShowType = InShowType;

	FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
	if (pCustomPresent)
	{
		bool ReadyToPush = IsLoadingIconMode();

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
					if (IsLoadingIconMode())
					{
						pPlugin->SetLoadingIconTexture(SplashScreenDescs[i].LoadingTexture->Resource->TextureRHI);
					}
					else
					{
						FRenderSplashInfo RenSplash;
						// use X (depth) as layers priority
						const uint32 Prio = FHMDLayerDesc::MaxPriority - uint32(SplashScreenDescs[i].TransformInMeters.GetTranslation().X * 1000.f);
						TSharedPtr<FHMDLayerDesc> layer = LayerMgr->AddLayer(FHMDLayerDesc::Quad, Prio, FHMDLayerManager::Layer_TorsoLocked, RenSplash.SplashLID);
						check(layer.IsValid());
						layer->SetTexture(SplashScreenDescs[i].LoadingTexture->Resource->TextureRHI);
						layer->SetTransform(SplashScreenDescs[i].TransformInMeters);
						layer->SetQuadSize(SplashScreenDescs[i].QuadSizeInMeters);

						RenSplash.Desc = SplashScreenDescs[i];

						FScopeLock ScopeLock2(&RenderSplashScreensLock);
						RenderSplashScreens.Add(RenSplash);
					}
					ReadyToPush = true;
				}
			}
		}

		// this will push black frame, if texture is not loaded
		pPlugin->EnterVRMode();

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
		UE_LOG(LogHMD, Log, TEXT("FGearVRSplash::Show"));
		SplashIsShown = true;
	}
}

void FGearVRSplash::PushFrame()
{
	check(IsInGameThread());

	if (IsLoadingIconMode())
	{
		UE_LOG(LogHMD, Log, TEXT("FGearVRSplash::PushFrame, loading icon"));

		pPlugin->SetLoadingIconMode(true);

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(SubmitSplash,
		FGearVR*, pGearVR, pPlugin,
		{
			pGearVR->RenderLoadingIcon_RenderThread();
		});
	}
	else
	{
		UE_LOG(LogHMD, Log, TEXT("FGearVRSplash::PushFrame"));

		FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
		check(pCustomPresent);
		if (pCustomPresent && pPlugin->HasValidOvrMobile())
		{
			// Create a fake frame to pass it to layer manager
			TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> _CurrentFrame = pPlugin->CreateNewGameFrame();
			FGameFrame* CurrentFrame = static_cast<FGameFrame*>(_CurrentFrame.Get());
			CurrentFrame->Settings = pPlugin->GetSettings()->Clone();
			CurrentFrame->FrameNumber = pPlugin->GetCurrentFrameNumber(); // make sure no 0 frame is used.
			// keep units in meters rather than UU (because UU make not much sense).
			CurrentFrame->Settings->WorldToMetersScale = 1.0f;
			CurrentFrame->SetWorldToMetersScale(CurrentFrame->Settings->WorldToMetersScale);
			CurrentFrame->GameThreadId = 0;//gettid();
			
			FSettings* CurrentSettings = static_cast<FSettings*>(CurrentFrame->Settings.Get());
			CurrentSettings->GpuLevel = 1;
			CurrentSettings->CpuLevel = 2;

			struct FSplashRenParams
			{
				FCustomPresent*	pCustomPresent;
				FHMDGameFramePtr		CurrentFrame;
				FHMDGameFramePtr*		RenderFramePtr;
			};

			FSplashRenParams params;
			params.pCustomPresent = pCustomPresent;
			params.CurrentFrame = _CurrentFrame;
			params.RenderFramePtr = &RenderFrame;

			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(SubmitSplash,
			const FSplashRenParams&, Params, params,
			FLayerManager*, LayerMgr, (FLayerManager*)(LayerMgr.Get()),
			{
				*Params.RenderFramePtr = Params.CurrentFrame;

				auto pCurrentFrame = static_cast<FGameFrame*>(Params.CurrentFrame.Get());
				FOvrMobileSynced OvrMobile(Params.pCustomPresent->GetMobileSynced());
				LayerMgr->PreSubmitUpdate_RenderThread(RHICmdList, pCurrentFrame, false);

				Params.pCustomPresent->PushFrame(LayerMgr, pCurrentFrame);
			});
			FlushRenderingCommands();
		}
	}
}

void FGearVRSplash::PushBlackFrame()
{
	check(IsInGameThread());
	ShowingBlack = true;
	LayerMgr->RemoveAllLayers();

	pPlugin->PushBlackFinal();

	UE_LOG(LogHMD, Log, TEXT("FGearVRSplash::PushBlackFrame"));
}

void FGearVRSplash::UnloadTextures()
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

void FGearVRSplash::Hide(EShowType InShowType)
{
	UE_LOG(LogHMD, Log, TEXT("FGearVRSplash::Hide"));
//	return; //?
#if 1
	check(IsInGameThread());
	if ((InShowType == ShowManually || InShowType == ShowType) && SplashIsShown)
	{
		UE_LOG(LogHMD, Log, TEXT("FGearVRSplash::Hide"));
		{
			FScopeLock ScopeLock2(&RenderSplashScreensLock);
			for (int32 i = 0; i < RenderSplashScreens.Num(); ++i)
			{
				LayerMgr->RemoveLayer(RenderSplashScreens[i].SplashLID);
			}
			RenderSplashScreens.SetNum(0);
		}
		UnloadTextures();

		if (IsLoadingIconMode())
		{
			pPlugin->SetLoadingIconMode(false);
		}

		SplashIsShown = false;

		PushBlackFrame();

		// keep in VR mode? @revise
		pPlugin->LeaveVRMode();

		FCustomPresent* pCustomPresent = pPlugin->GetCustomPresent_Internal();
		if (pCustomPresent)
		{
			pCustomPresent->UnlockSubmitFrame();
		}
		ShowType = None;
	}
#endif
}

void FGearVRSplash::SetLoadingIconMode(bool bInLoadingIconMode)
{
	check(!SplashIsShown); // do not call this when splash is showing, otherwise there will be racing condition with the Tick
	FAsyncLoadingSplash::SetLoadingIconMode(bInLoadingIconMode);
}

#endif // GEARVR_SUPPORTED_PLATFORMS

