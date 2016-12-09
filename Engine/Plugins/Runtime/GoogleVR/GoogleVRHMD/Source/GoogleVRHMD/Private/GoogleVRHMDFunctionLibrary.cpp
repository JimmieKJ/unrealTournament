/* Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Classes/GoogleVRHMDFunctionLibrary.h"
#include "GoogleVRHMD.h"

UGoogleVRHMDFunctionLibrary::UGoogleVRHMDFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

static FGoogleVRHMD* GetHMD()
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->GetVersionString().Contains(TEXT("GoogleVR")) )
	{
		return static_cast<FGoogleVRHMD*>(GEngine->HMDDevice.Get());
	}

	return nullptr;
}

bool UGoogleVRHMDFunctionLibrary::IsGoogleVRHMDEnabled()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->IsHMDEnabled();
	}
	return false;
}

bool UGoogleVRHMDFunctionLibrary::IsGoogleVRStereoRenderingEnabled()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->IsStereoEnabled();
	}
	return false;
}

void UGoogleVRHMDFunctionLibrary::SetDistortionCorrectionEnabled(bool bEnable)
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		HMD->SetDistortionCorrectionEnabled(bEnable);
	}
}

bool UGoogleVRHMDFunctionLibrary::SetDefaultViewerProfile(const FString& ViewerProfileURL)
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->SetDefaultViewerProfile(ViewerProfileURL);
	}

	return false;
}

void UGoogleVRHMDFunctionLibrary::SetDistortionMeshSize(EDistortionMeshSizeEnum MeshSize)
{
	FGoogleVRHMD* HMD = GetHMD();
	if (HMD)
	{
		HMD->SetDistortionMeshSize(MeshSize);
	}
}

bool UGoogleVRHMDFunctionLibrary::GetDistortionCorrectionEnabled()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->GetDistortionCorrectionEnabled();
	}

	return false;
}

FString UGoogleVRHMDFunctionLibrary::GetViewerModel()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->GetViewerModel();
	}

	return TEXT("");
}

FString UGoogleVRHMDFunctionLibrary::GetViewerVendor()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->GetViewerVendor();
	}

	return TEXT("");
}

bool UGoogleVRHMDFunctionLibrary::IsVrLaunch()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->IsVrLaunch();
	}

	return false;
}

FIntPoint UGoogleVRHMDFunctionLibrary::GetGVRHMDRenderTargetSize()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->GetGVRHMDRenderTargetSize();
	}

	return FIntPoint::ZeroValue;
}

FIntPoint UGoogleVRHMDFunctionLibrary::SetRenderTargetSizeToDefault()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->SetRenderTargetSizeToDefault();
	}

	return FIntPoint::ZeroValue;
}

bool UGoogleVRHMDFunctionLibrary::SetGVRHMDRenderTargetScale(float ScaleFactor, FIntPoint& OutRenderTargetSize)
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->SetGVRHMDRenderTargetSize(ScaleFactor, OutRenderTargetSize);
	}

	return false;
}

bool UGoogleVRHMDFunctionLibrary::SetGVRHMDRenderTargetSize(int DesiredWidth, int DesiredHeight, FIntPoint& OutRenderTargetSize)
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->SetGVRHMDRenderTargetSize(DesiredWidth, DesiredHeight, OutRenderTargetSize);
	}

	return false;
}

void UGoogleVRHMDFunctionLibrary::SetNeckModelScale(float ScaleFactor)
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->SetNeckModelScale(ScaleFactor);
	}
}

float UGoogleVRHMDFunctionLibrary::GetNeckModelScale()
{
	FGoogleVRHMD* HMD = GetHMD();
	if (HMD)
	{
		return HMD->GetNeckModelScale();
	}

	return 0.0f;
}

FString UGoogleVRHMDFunctionLibrary::GetIntentData()
{
	FGoogleVRHMD* HMD = GetHMD();
	if (HMD)
	{
		return HMD->GetIntentData();
	}

	return TEXT("");
}

void UGoogleVRHMDFunctionLibrary::SetDaydreamLoadingSplashScreenEnable(bool bEnable)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD && HMD->GVRSplash.IsValid())
	{
		HMD->GVRSplash->bEnableSplashScreen = bEnable;
	}
#endif
}

void UGoogleVRHMDFunctionLibrary::SetDaydreamLoadingSplashScreenTexture(UTexture2D* Texture, FVector2D UVOffset, FVector2D UVSize)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD && HMD->GVRSplash.IsValid() && Texture)
	{
		HMD->GVRSplash->SplashTexture = Texture;
		HMD->GVRSplash->SplashTexturePath = "";
		HMD->GVRSplash->SplashTextureUVOffset = UVOffset;
		HMD->GVRSplash->SplashTextureUVSize = UVSize;
	}
#endif
}

void UGoogleVRHMDFunctionLibrary::ClearDaydreamLoadingSplashScreenTexture()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD && HMD->GVRSplash.IsValid())
	{
		HMD->GVRSplash->SplashTexture = nullptr;
		HMD->GVRSplash->SplashTexturePath = "";
	}
#endif
}