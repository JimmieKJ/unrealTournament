// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "Kismet/StereoLayerFunctionLibrary.h"
#include "EngineGlobals.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "Engine/Engine.h"
#include "IStereoLayers.h"
#include "IHeadMountedDisplay.h"


static IStereoLayers* GetStereoLayers()
{
	if (GEngine && GEngine->HMDDevice.IsValid())
	{
		return GEngine->HMDDevice->GetStereoLayers();
	}

	return nullptr;
}

class FAutoShow: public TSharedFromThis<FAutoShow>
{
public:
	void OnPreLoadMap(const FString&);
	void OnPostLoadMap();

	void Register();
	void Unregister();
};

void FAutoShow::OnPreLoadMap(const FString&)
{
	IStereoLayers* StereoLayers = GetStereoLayers();
	if (StereoLayers)
	{
		StereoLayers->ShowSplashScreen();
	}
}

void FAutoShow::OnPostLoadMap()
{
	IStereoLayers* StereoLayers = GetStereoLayers();
	if (StereoLayers)
	{
		StereoLayers->HideSplashScreen();
	}
}

void FAutoShow::Register()
{
	FCoreUObjectDelegates::PreLoadMap.AddSP(this, &FAutoShow::OnPreLoadMap);
	FCoreUObjectDelegates::PostLoadMap.AddSP(this, &FAutoShow::OnPostLoadMap);
}

void FAutoShow::Unregister()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMap.RemoveAll(this);
}

static TSharedPtr<FAutoShow> AutoShow;

UStereoLayerFunctionLibrary::UStereoLayerFunctionLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UStereoLayerFunctionLibrary::SetSplashScreen(class UTexture* Texture, FVector2D Scale, FVector2D Offset, bool bShowLoadingMovie, bool bShowOnSet)
{
	IStereoLayers* StereoLayers = GetStereoLayers();
	if (StereoLayers && Texture && Texture->Resource)
	{
		StereoLayers->SetSplashScreen(Texture->Resource->TextureRHI, Scale, Offset, bShowLoadingMovie);
		if (bShowOnSet)
		{
			StereoLayers->ShowSplashScreen();
		}
	}
}

void UStereoLayerFunctionLibrary::ShowSplashScreen()
{
	IStereoLayers* StereoLayers = GetStereoLayers();
	if (StereoLayers)
	{
		StereoLayers->ShowSplashScreen();
	}
}

void UStereoLayerFunctionLibrary::HideSplashScreen()
{
	IStereoLayers* StereoLayers = GetStereoLayers();
	if (StereoLayers)
	{
		StereoLayers->HideSplashScreen();
	}
}

void UStereoLayerFunctionLibrary::EnableAutoLoadingSplashScreen(bool InAutoShowEnabled)
{
	if (InAutoShowEnabled)
	{
		AutoShow = MakeShareable(new FAutoShow);
		AutoShow->Register();
	}
	else if (AutoShow.IsValid())
	{
		AutoShow->Unregister();
		AutoShow = nullptr;
	}
}
