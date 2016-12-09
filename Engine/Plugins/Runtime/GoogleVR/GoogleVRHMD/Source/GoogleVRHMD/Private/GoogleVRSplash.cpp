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

#include "GoogleVRSplash.h"

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
#include "GoogleVRHMD.h"
#include "ScreenRendering.h"

FGoogleVRSplash::FGoogleVRSplash(FGoogleVRHMD* InGVRHMD)
	: bEnableSplashScreen(true)
	, SplashTexture(nullptr)
	, GVRHMD(InGVRHMD)
	, RendererModule(nullptr)
	, GVRCustomPresent(nullptr)
	, bInitialized(false)
	, bIsShown(false)
	, bSplashScreenRendered(false)
{
	// Make sure we have a valid GoogleVRHMD plugin;
	check(GVRHMD && GVRHMD->CustomPresent);

	RendererModule = GVRHMD->RendererModule;
	GVRCustomPresent = GVRHMD->CustomPresent;
}

FGoogleVRSplash::~FGoogleVRSplash()
{
	Hide();

	if (bInitialized)
	{
		FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
		FCoreUObjectDelegates::PostLoadMap.RemoveAll(this);
	}
}

void FGoogleVRSplash::Init()
{
	if (!bInitialized)
	{
		FCoreUObjectDelegates::PreLoadMap.AddSP(this, &FGoogleVRSplash::OnPreLoadMap);
		FCoreUObjectDelegates::PostLoadMap.AddSP(this, &FGoogleVRSplash::OnPostLoadMap);

		LoadDefaultSplashTexturePath();
		bInitialized = true;
	}
}

void FGoogleVRSplash::OnPreLoadMap(const FString&)
{
	Show();
}

void FGoogleVRSplash::OnPostLoadMap()
{
	Hide();
}

void FGoogleVRSplash::AllocateSplashScreenRenderTarget()
{
	if (!GVRCustomPresent->TextureSet.IsValid())
	{
		GVRCustomPresent->AllocateRenderTargetTexture(0, GVRHMD->GVRRenderTargetSize.X, GVRHMD->GVRRenderTargetSize.Y, PF_B8G8R8A8, 1, TexCreate_None, TexCreate_RenderTargetable);
	}
}

void FGoogleVRSplash::Show()
{
	check(IsInGameThread());

	if (!bEnableSplashScreen || bIsShown)
	{
		return;
	}

	// Load the splash screen texture if it is specified from the path.
	if (!SplashTexturePath.IsEmpty())
	{
		LoadTexture();
	}

	bSplashScreenRendered = false;

	RenderThreadTicker = MakeShareable(new FGoogleVRSplashTicker(this));
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(RegisterAsyncTick,
	FTickableObjectRenderThread*, RenderThreadTicker, RenderThreadTicker.Get(),
	FGoogleVRSplash*, pGVRSplash, this,
	{
		pGVRSplash->AllocateSplashScreenRenderTarget();
		RenderThreadTicker->Register();
	});

	bIsShown = true;
}

void FGoogleVRSplash::Hide()
{
	check(IsInGameThread());

	if (!bIsShown)
	{
		return;
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(UnregisterAsyncTick,
	TSharedPtr<FGoogleVRSplashTicker>&, RenderThreadTicker, RenderThreadTicker,
	FGoogleVRSplash*, pGVRSplash, this,
	{
		pGVRSplash->SubmitBlackFrame();

		RenderThreadTicker->Unregister();
		RenderThreadTicker = nullptr;
	});
	FlushRenderingCommands();

	if (!SplashTexturePath.IsEmpty())
	{
		UnloadTexture();
	}

	bIsShown = false;
}

void FGoogleVRSplash::Tick(float DeltaTime)
{
	check(IsInRenderingThread());

	// Update Resources (ex. render, copy, etc.)

	if(!bSplashScreenRendered)
	{
		// Make sure we have a valid render target
		check(GVRCustomPresent->TextureSet.IsValid());

		GVRHMD->UpdateHeadPose();
		GVRHMD->UpdateGVRViewportList();

		// Bind GVR RenderTarget
		GVRCustomPresent->BeginRendering(GVRHMD->CachedHeadPose);
		RenderStereoSplashScreen(FRHICommandListExecutor::GetImmediateCommandList(), GVRCustomPresent->TextureSet->GetTexture2D());
		GVRCustomPresent->FinishRendering();

		bSplashScreenRendered = true;
	}
}

bool FGoogleVRSplash::IsTickable() const
{
	return bIsShown;
}

void FGoogleVRSplash::RenderStereoSplashScreen(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef DstTexture)
{
	check(IsInRenderingThread());

	const uint32 ViewportWidth = DstTexture->GetSizeX();
	const uint32 ViewportHeight = DstTexture->GetSizeY();

	FIntRect DstRect = FIntRect(0, 0, ViewportWidth, ViewportHeight);

	SetRenderTarget(RHICmdList, DstTexture, FTextureRHIRef());
	RHICmdList.ClearColorTexture(DstTexture, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), FIntRect());

	// If the texture is not avaliable, we just clear the DstTexture with black.
	if (SplashTexture &&  SplashTexture->IsValidLowLevel())
	{
		FRHITexture* SrcTextureRHI = SplashTexture->Resource->TextureRHI;

		RHICmdList.SetViewport(DstRect.Min.X, DstRect.Min.Y, 0, DstRect.Max.X, DstRect.Max.Y, 1.0f);
		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		const auto FeatureLevel = GMaxRHIFeatureLevel;
		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTextureRHI);

		// Modify the v to flip the texture. Somehow it renders flipped if not.
		float U = SplashTextureUVOffset.X;
		float V = SplashTextureUVOffset.Y + SplashTextureUVSize.Y;
		float USize = SplashTextureUVSize.X;
		float VSize = -SplashTextureUVSize.Y;

		// Render left eye texture
		RendererModule->DrawRectangle(
			RHICmdList,
			0, 0,
			ViewportWidth / 2, ViewportHeight,
			U, V,
			USize, VSize,
			FIntPoint(ViewportWidth, ViewportHeight),
			FIntPoint(1, 1),
			*VertexShader,
			EDRF_Default);

		// Render right eye texture
		RendererModule->DrawRectangle(
			RHICmdList,
			ViewportWidth / 2, 0,
			ViewportWidth / 2, ViewportHeight,
			U, V,
			USize, VSize,
			FIntPoint(ViewportWidth, ViewportHeight),
			FIntPoint(1, 1),
			*VertexShader,
			EDRF_Default);
	}
}

void FGoogleVRSplash::SubmitBlackFrame()
{
	check(IsInRenderingThread());

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FTexture2DRHIParamRef DstTexture = GVRCustomPresent->TextureSet->GetTexture2D();

	GVRCustomPresent->BeginRendering(GVRHMD->CachedHeadPose);

	SetRenderTarget(RHICmdList, DstTexture, FTextureRHIRef());
	RHICmdList.ClearColorTexture(DstTexture, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), FIntRect());

	GVRCustomPresent->FinishRendering();
}

bool FGoogleVRSplash::LoadDefaultSplashTexturePath()
{
	SplashTexturePath = "";
	SplashTextureUVOffset = FVector2D(0.0f, 0.0f);
	SplashTextureUVSize = FVector2D(1.0f, 1.0f);

	const TCHAR* SplashSettings = TEXT("Daydream.Splash.Settings");
	GConfig->GetString(SplashSettings, *FString("TexturePath"), SplashTexturePath, GEngineIni);
	GConfig->GetVector2D(SplashSettings, *FString("TextureUVOffset"), SplashTextureUVOffset, GEngineIni);
	GConfig->GetVector2D(SplashSettings, *FString("TextureUVSize"), SplashTextureUVSize, GEngineIni);

	UE_LOG(LogHMD, Log, TEXT("Set default splash texture path to %s from GEngineIni."), *SplashTexturePath);
	UE_LOG(LogHMD, Log, TEXT("Set default splash texture UV offset to (%f, %f) from GEngineIni."), SplashTextureUVOffset.X, SplashTextureUVOffset.Y);
	UE_LOG(LogHMD, Log, TEXT("Set default splash texture UV size to (%f, %f) from GEngineIni."), SplashTextureUVSize.X, SplashTextureUVSize.Y);

	return true;
}

bool FGoogleVRSplash::LoadTexture()
{
	SplashTexture = LoadObject<UTexture2D>(nullptr, *SplashTexturePath, nullptr, LOAD_None, nullptr);

	if (SplashTexture && SplashTexture->IsValidLowLevel())
	{
		UE_LOG(LogHMD, Log, TEXT("Splash Texture load successful!"));
		SplashTexture->UpdateResource();
		FlushRenderingCommands();
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("Failed to load the Splash Texture at path %s"), *SplashTexturePath);
	}

	return SplashTexture != nullptr;
}

void FGoogleVRSplash::UnloadTexture()
{
	if (SplashTexture && SplashTexture->IsValidLowLevel())
	{
		SplashTexture = nullptr;
	}
}
#endif //GOOGLEVRHMD_SUPPORTED_PLATFORMS