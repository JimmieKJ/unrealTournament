// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "HMDPrivatePCH.h"
#include "OculusRiftHMD.h"

#if !PLATFORM_MAC // Mac uses 0.5/OculusRiftRender_05.cpp

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"
#include "ScreenRendering.h"

#include "SlateBasics.h"


// STATGROUP_OculusRiftHMD is declared in OculusRiftHMD.h
DEFINE_STAT(STAT_BeginRendering); // see OculusRiftHMD.h
DECLARE_CYCLE_STAT(TEXT("FinishRendering"), STAT_FinishRendering, STATGROUP_OculusRiftHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("LatencyRender"), STAT_LatencyRender, STATGROUP_OculusRiftHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("LatencyTimewarp"), STAT_LatencyTimewarp, STATGROUP_OculusRiftHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("LatencyPostPresent"), STAT_LatencyPostPresent, STATGROUP_OculusRiftHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("ErrorRender"), STAT_ErrorRender, STATGROUP_OculusRiftHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("ErrorTimewarp"), STAT_ErrorTimewarp, STATGROUP_OculusRiftHMD);

//-------------------------------------------------------------------------------------------------
// FViewExtension
//-------------------------------------------------------------------------------------------------

void FViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());
	FViewExtension& RenderContext = *this;
	FGameFrame* CurrentFrame = static_cast<FGameFrame*>(RenderContext.RenderFrame.Get());

	if (bFrameBegun || !CurrentFrame || !CurrentFrame->Settings->IsStereoEnabled() || !ViewFamily.RenderTarget->GetRenderTargetTexture())
	{
		return;
	}
	FSettings* FrameSettings = CurrentFrame->GetSettings();
	RenderContext.ShowFlags = ViewFamily.EngineShowFlags;

	FMemory::Memcpy(CurrentFrame->CurEyeRenderPose, CurrentFrame->EyeRenderPose);
	FMemory::Memcpy(CurrentFrame->CurHeadPose, CurrentFrame->HeadPose);


	if (FrameSettings->TexturePaddingPerEye != 0)
	{
		// clear the padding between two eyes
		const int32 GapMinX = ViewFamily.Views[0]->ViewRect.Max.X;
		const int32 GapMaxX = ViewFamily.Views[1]->ViewRect.Min.X;

		const int ViewportSizeY = (ViewFamily.RenderTarget->GetRenderTargetTexture()) ? 
			ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeY() : ViewFamily.RenderTarget->GetSizeXY().Y;
		RHICmdList.SetViewport(GapMinX, 0, 0, GapMaxX, ViewportSizeY, 1.0f);
		RHICmdList.Clear(true, FLinearColor::Black, false, 0, false, 0, FIntRect());
	}

	check(ViewFamily.RenderTarget->GetRenderTargetTexture());
	
	pPresentBridge->BeginRendering(RenderContext, ViewFamily.RenderTarget->GetRenderTargetTexture());

	const double DisplayTime = ovr_GetPredictedDisplayTime(OvrSession, RenderContext.RenderFrame->FrameNumber);

	RenderContext.bFrameBegun = true;

	// Update FPS stats
	FOculusRiftHMD* OculusRiftHMD = static_cast<FOculusRiftHMD*>(RenderContext.Delegate);

	OculusRiftHMD->PerformanceStats.Frames++;
	OculusRiftHMD->PerformanceStats.Seconds += DisplayTime;

	if (RenderContext.ShowFlags.Rendering)
	{
		// Take new EyeRenderPose if bUpdateOnRT.
		// if !bOrientationChanged && !bPositionChanged then we still need to use new eye pose (for timewarp)
		if (FrameSettings->Flags.bUpdateOnRT || 
			(!CurrentFrame->Flags.bOrientationChanged && !CurrentFrame->Flags.bPositionChanged))
		{
			// get latest orientation/position and cache it
			CurrentFrame->GetHeadAndEyePoses(CurrentFrame->GetTrackingState(OvrSession), CurrentFrame->CurHeadPose, CurrentFrame->CurEyeRenderPose);
		}
	}

	FQuat OldOrientation = CurrentFrame->LastHmdOrientation;
	FVector OldPosition = CurrentFrame->LastHmdPosition;
	const FTransform OldRelativeTransform(OldOrientation, OldPosition);

	FQuat NewOrientation;
	FVector NewPosition;
	CurrentFrame->PoseToOrientationAndPosition(CurrentFrame->CurHeadPose, NewOrientation, NewPosition);
	const FTransform NewRelativeTransform(NewOrientation, NewPosition);

	Delegate->ApplyLateUpdate(ViewFamily.Scene, OldRelativeTransform, NewRelativeTransform);
}

void FViewExtension::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View)
{
	check(IsInRenderingThread());
	FViewExtension& RenderContext = *this;
	FGameFrame* CurrentFrame = static_cast<FGameFrame*>(RenderContext.RenderFrame.Get());

	if (!CurrentFrame || !CurrentFrame->Settings->IsStereoEnabled())
	{
		return;
	}

	const ovrEyeType eyeIdx = (View.StereoPass == eSSP_LEFT_EYE) ? ovrEye_Left : ovrEye_Right;
	if (RenderContext.ShowFlags.Rendering && CurrentFrame->Settings->Flags.bUpdateOnRT)
	{
		FQuat	CurrentEyeOrientation;
		FVector	CurrentEyePosition;
		CurrentFrame->PoseToOrientationAndPosition(CurrentFrame->CurEyeRenderPose[eyeIdx], CurrentEyeOrientation, CurrentEyePosition);

		FQuat ViewOrientation = View.ViewRotation.Quaternion();

		// recalculate delta control orientation; it should match the one we used in CalculateStereoViewOffset on a game thread.
		FVector GameEyePosition;
		FQuat GameEyeOrient;

		CurrentFrame->PoseToOrientationAndPosition(CurrentFrame->EyeRenderPose[eyeIdx], GameEyeOrient, GameEyePosition);
		const FQuat DeltaControlOrientation =  ViewOrientation * GameEyeOrient.Inverse();
		// make sure we use the same viewrotation as we had on a game thread
		check(View.ViewRotation == CurrentFrame->CachedViewRotation[eyeIdx]);

		if (CurrentFrame->Flags.bOrientationChanged)
		{
			// Apply updated orientation to corresponding View at recalc matrices.
			// The updated position will be applied from inside of the UpdateViewMatrix() call.
			const FQuat DeltaOrient = View.BaseHmdOrientation.Inverse() * CurrentEyeOrientation;
			View.ViewRotation = FRotator(ViewOrientation * DeltaOrient);
			
			//UE_LOG(LogHMD, Log, TEXT("VIEWDLT: Yaw %.3f Pitch %.3f Roll %.3f"), DeltaOrient.Rotator().Yaw, DeltaOrient.Rotator().Pitch, DeltaOrient.Rotator().Roll);
		}

		if (!CurrentFrame->Flags.bPositionChanged)
		{
			// if no positional change applied then we still need to calculate proper stereo disparity.
			// use the current head pose for this calculation instead of the one that was saved on a game thread.
			FQuat HeadOrientation;
			CurrentFrame->PoseToOrientationAndPosition(CurrentFrame->CurHeadPose, HeadOrientation, View.BaseHmdLocation);
		}

		// The HMDPosition already has HMD orientation applied.
		// Apply rotational difference between HMD orientation and ViewRotation
		// to HMDPosition vector. 
		const FVector DeltaPosition = CurrentEyePosition - View.BaseHmdLocation;
		const FVector vEyePosition = DeltaControlOrientation.RotateVector(DeltaPosition) + CurrentFrame->Settings->PositionOffset;
		View.ViewLocation += vEyePosition;

		//UE_LOG(LogHMD, Log, TEXT("VDLTPOS: %.3f %.3f %.3f"), vEyePosition.X, vEyePosition.Y, vEyePosition.Z);

		if (CurrentFrame->Flags.bOrientationChanged || CurrentFrame->Flags.bPositionChanged)
		{
			View.UpdateViewMatrix();
		}

		if (View.StereoPass == eSSP_LEFT_EYE) // collect PlayerOrientation only once
		{
			FQuat HeadOrientation;
			FVector HeadLocation;
			CurrentFrame->PoseToOrientationAndPosition(CurrentFrame->CurHeadPose, HeadOrientation, HeadLocation);

			CurrentFrame->PlayerOrientation = ViewOrientation * HeadOrientation.Inverse();
			//UE_LOG(LogHMD, Log, TEXT("PLAYER: Pos %.3f %.3f %.3f"), CurrentFrame->PlayerLocation.X, CurrentFrame->PlayerLocation.Y, CurrentFrame->PlayerLocation.Z);
			//UE_LOG(LogHMD, Log, TEXT("VIEW  : Yaw %.3f Pitch %.3f Roll %.3f"), FRotator(ViewOrientation).Yaw, FRotator(ViewOrientation).Pitch, FRotator(ViewOrientation).Roll);
			//UE_LOG(LogHMD, Log, TEXT("HEAD  : Yaw %.3f Pitch %.3f Roll %.3f"), FRotator(HeadOrientation).Yaw, FRotator(HeadOrientation).Pitch, FRotator(HeadOrientation).Roll);
			//UE_LOG(LogHMD, Log, TEXT("PLAYER: Yaw %.3f Pitch %.3f Roll %.3f"), FRotator(CurrentFrame->PlayerOrientation).Yaw, FRotator(CurrentFrame->PlayerOrientation).Pitch, FRotator(CurrentFrame->PlayerOrientation).Roll);
		}
	}
}


//-------------------------------------------------------------------------------------------------
// FOculusRiftHMD
//-------------------------------------------------------------------------------------------------

bool FOculusRiftHMD::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InFlags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	check(Index == 0);
	return pCustomPresent->AllocateRenderTargetTexture(SizeX, SizeY, Format, NumMips, InFlags, TargetableTextureFlags, OutTargetableTexture, OutShaderResourceTexture, NumSamples);
}

void FCustomPresent::CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef DstTexture, FTexture2DRHIParamRef SrcTexture, 
	FIntRect DstRect, FIntRect SrcRect) const
{
	check(IsInRenderingThread());

	if (DstRect.IsEmpty())
	{
		DstRect = FIntRect(0, 0, DstTexture->GetSizeX(), DstTexture->GetSizeY());
	}
	const uint32 ViewportWidth = DstRect.Width();
	const uint32 ViewportHeight = DstRect.Height();
	const FIntPoint TargetSize(ViewportWidth, ViewportHeight);

	const float SrcTextureWidth = SrcTexture->GetSizeX();
	const float SrcTextureHeight = SrcTexture->GetSizeY();
	float U = 0.f, V = 0.f, USize = 1.f, VSize = 1.f;
	if (!SrcRect.IsEmpty())
	{
		U = SrcRect.Min.X / SrcTextureWidth;
		V = SrcRect.Min.Y / SrcTextureHeight;
		USize = SrcRect.Width() / SrcTextureWidth;
		VSize = SrcRect.Height() / SrcTextureHeight;
	}

	FRHITexture* SrcTextureRHI = SrcTexture;
	RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, &SrcTextureRHI, 1);

	SetRenderTarget(RHICmdList, DstTexture, FTextureRHIRef());
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

	RendererModule->DrawRectangle(
		RHICmdList,
		0, 0,
		ViewportWidth, ViewportHeight,
		U, V,
		USize, VSize,
		TargetSize,
		FIntPoint(1, 1),
		*VertexShader,
		EDRF_Default);
}

void FOculusRiftHMD::RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture) const
{
	check(IsInRenderingThread());

	check(pCustomPresent);

	pCustomPresent->UpdateLayers(RHICmdList);

	auto RenderContext = pCustomPresent->GetRenderContext();
	if (RenderContext && RenderContext->GetFrameSettings()->Flags.bMirrorToWindow)
	{
		if (RenderContext->GetFrameSettings()->MirrorWindowMode == FSettings::eMirrorWindow_Distorted)
		{
			FTexture2DRHIRef MirrorTexture = pCustomPresent->GetMirrorTexture();
			if (MirrorTexture)
			{
				pCustomPresent->CopyTexture_RenderThread(RHICmdList, BackBuffer, MirrorTexture);
			}
		}
		else if (RenderContext->GetFrameSettings()->MirrorWindowMode == FSettings::eMirrorWindow_Undistorted)
		{
			auto FrameSettings = RenderContext->GetFrameSettings();
			FIntRect destRect(0, 0, BackBuffer->GetSizeX() / 2, BackBuffer->GetSizeY());
			for (int i = 0; i < 2; ++i)
			{
				pCustomPresent->CopyTexture_RenderThread(RHICmdList, BackBuffer, SrcTexture, destRect, FrameSettings->EyeRenderViewport[i]);
				destRect.Min.X += BackBuffer->GetSizeX() / 2;
				destRect.Max.X += BackBuffer->GetSizeX() / 2;
			}
		}
		else if (RenderContext->GetFrameSettings()->MirrorWindowMode == FSettings::eMirrorWindow_SingleEye)
		{
			auto FrameSettings = RenderContext->GetFrameSettings();
			pCustomPresent->CopyTexture_RenderThread(RHICmdList, BackBuffer, SrcTexture, FIntRect(), FrameSettings->EyeRenderViewport[0]);
		}
	}
}

static void DrawOcclusionMesh(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass, const FHMDViewMesh MeshAssets[])
{
	check(IsInRenderingThread());
	check(StereoPass != eSSP_FULL);

	const uint32 MeshIndex = (StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
	const FHMDViewMesh& Mesh = MeshAssets[MeshIndex];
	check(Mesh.IsValid());

	DrawIndexedPrimitiveUP(
		RHICmdList,
		PT_TriangleList,
		0,
		Mesh.NumVertices,
		Mesh.NumTriangles,
		Mesh.pIndices,
		sizeof(Mesh.pIndices[0]),
		Mesh.pVertices,
		sizeof(Mesh.pVertices[0])
		);
}

void FOculusRiftHMD::DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	DrawOcclusionMesh(RHICmdList, StereoPass, HiddenAreaMeshes);
}

void FOculusRiftHMD::DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	DrawOcclusionMesh(RHICmdList, StereoPass, VisibleAreaMeshes);
}

void FOculusRiftHMD::CalculateRenderTargetSize(const FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	check(IsInGameThread());

	if (!Settings->IsStereoEnabled())
	{
		return;
	}

	// We must be sure the rendertargetsize is calculated already
	if (Flags.bNeedUpdateStereoRenderingParams)
	{
		UpdateStereoRenderingParams();
	}

	InOutSizeX = GetSettings()->RenderTargetSize.X;
	InOutSizeY = GetSettings()->RenderTargetSize.Y;

	check(InOutSizeX != 0 && InOutSizeY != 0);
}

bool FOculusRiftHMD::NeedReAllocateViewportRenderTarget(const FViewport& Viewport)
{
	check(IsInGameThread());
	if (Settings->IsStereoEnabled())
	{
		const uint32 InSizeX = Viewport.GetSizeXY().X;
		const uint32 InSizeY = Viewport.GetSizeXY().Y;
		const FIntPoint RenderTargetSize = Viewport.GetRenderTargetTextureSizeXY();

		uint32 NewSizeX = InSizeX, NewSizeY = InSizeY;
		CalculateRenderTargetSize(Viewport, NewSizeX, NewSizeY);
		if (NewSizeX != RenderTargetSize.X || NewSizeY != RenderTargetSize.Y || (pCustomPresent && pCustomPresent->AreTexturesMarkedAsInvalid()))
		{
			return true;
		}
	}
	return false;
}

static const char* FormatLatencyReading(char* buff, size_t size, float val)
{
	if (val < 0.000001f)
	{
		FCStringAnsi::Strcpy(buff, size, "N/A   ");
	}
	else
	{
		FCStringAnsi::Snprintf(buff, size, "%4.2fms", val * 1000.0f);
	}
	return buff;
}

#if !UE_BUILD_SHIPPING
static void RenderLines(FCanvas* Canvas, int numLines, const FColor& c, float* x, float* y)
{
	for (int i = 0; i < numLines; ++i)
	{
		FCanvasLineItem line(FVector2D(x[i*2], y[i*2]), FVector2D(x[i*2+1], y[i*2+1]));
		line.SetColor(FLinearColor(c));
		Canvas->DrawItem(line);
	}
}
#endif // #if !UE_BUILD_SHIPPING

void FOculusRiftHMD::DrawDebug(UCanvas* Canvas)
{
#if !UE_BUILD_SHIPPING
	check(IsInGameThread());
	const auto frame = GetFrame();
	if (frame)
	{
		FSettings* FrameSettings = frame->GetSettings();

		if (FrameSettings->Flags.bDrawGrid)
		{
			bool bStereo = Canvas->Canvas->IsStereoRendering();
			Canvas->Canvas->SetStereoRendering(false);
			bool bPopTransform = false;
			if (FrameSettings->EyeRenderDesc[0].DistortedViewport.Size.w != FMath::CeilToInt(Canvas->ClipX) ||
				FrameSettings->EyeRenderDesc[0].DistortedViewport.Size.h != Canvas->ClipY)
			{
				// scale if resolution of the Canvas does not match the viewport
				bPopTransform = true;
				Canvas->Canvas->PushAbsoluteTransform(FScaleMatrix(
					FVector((Canvas->ClipX) / float(FrameSettings->EyeRenderDesc[0].DistortedViewport.Size.w),
					Canvas->ClipY / float(FrameSettings->EyeRenderDesc[0].DistortedViewport.Size.h),
					1.0f)));
			}

			const FColor cNormal(255, 0, 0);
			const FColor cSpacer(255, 255, 0);
			const FColor cMid(0, 128, 255);
			for (int eye = 0; eye < 2; ++eye)
			{
				int lineStep = 1;
				int midX = 0;
				int midY = 0;
				int limitX = 0;
				int limitY = 0;

				int renderViewportX = FrameSettings->EyeRenderDesc[eye].DistortedViewport.Pos.x;
				int renderViewportY = FrameSettings->EyeRenderDesc[eye].DistortedViewport.Pos.y;
				int renderViewportW = FrameSettings->EyeRenderDesc[eye].DistortedViewport.Size.w;
				int renderViewportH = FrameSettings->EyeRenderDesc[eye].DistortedViewport.Size.h;

				lineStep = 48;
				OVR::Vector2f rendertargetNDC = OVR::FovPort(FrameSettings->EyeRenderDesc[eye].Fov).TanAngleToRendertargetNDC(OVR::Vector2f(0.0f));
				midX = (int)((rendertargetNDC.x * 0.5f + 0.5f) * (float)renderViewportW + 0.5f);
				midY = (int)((rendertargetNDC.y * 0.5f + 0.5f) * (float)renderViewportH + 0.5f);
				limitX = FMath::Max(renderViewportW - midX, midX);
				limitY = FMath::Max(renderViewportH - midY, midY);

				int spacerMask = (lineStep << 1) - 1;

				for (int xp = 0; xp < limitX; xp += lineStep)
				{
					float x[4];
					float y[4];
					x[0] = (float)(midX + xp) + renderViewportX;
					y[0] = (float)0 + renderViewportY;
					x[1] = (float)(midX + xp) + renderViewportX;
					y[1] = (float)renderViewportH + renderViewportY;
					x[2] = (float)(midX - xp) + renderViewportX;
					y[2] = (float)0 + renderViewportY;
					x[3] = (float)(midX - xp) + renderViewportX;
					y[3] = (float)renderViewportH + renderViewportY;
					if (xp == 0)
					{
						RenderLines(Canvas->Canvas, 1, cMid, x, y);
					}
					else if ((xp & spacerMask) == 0)
					{
						RenderLines(Canvas->Canvas, 2, cSpacer, x, y);
					}
					else
					{
						RenderLines(Canvas->Canvas, 2, cNormal, x, y);
					}
				}
				for (int yp = 0; yp < limitY; yp += lineStep)
				{
					float x[4];
					float y[4];
					x[0] = (float)0 + renderViewportX;
					y[0] = (float)(midY + yp) + renderViewportY;
					x[1] = (float)renderViewportW + renderViewportX;
					y[1] = (float)(midY + yp) + renderViewportY;
					x[2] = (float)0 + renderViewportX;
					y[2] = (float)(midY - yp) + renderViewportY;
					x[3] = (float)renderViewportW + renderViewportX;
					y[3] = (float)(midY - yp) + renderViewportY;
					if (yp == 0)
					{
						RenderLines(Canvas->Canvas, 1, cMid, x, y);
					}
					else if ((yp & spacerMask) == 0)
					{
						RenderLines(Canvas->Canvas, 2, cSpacer, x, y);
					}
					else
					{
						RenderLines(Canvas->Canvas, 2, cNormal, x, y);
					}
				}
			}
			if (bPopTransform)
			{
				Canvas->Canvas->PopTransform(); // optional scaling
			}
			Canvas->Canvas->SetStereoRendering(bStereo);
		}
		if (IsStereoEnabled() && FrameSettings->Flags.bShowStats)
		{
			static const FColor TextColor(0,255,0);
			// Pick a larger font on console.
			UFont* const Font = FPlatformProperties::SupportsWindowedMode() ? GEngine->GetSmallFont() : GEngine->GetMediumFont();
			const int32 RowHeight = FMath::TruncToInt(Font->GetMaxCharHeight() * 1.1f);

			float ClipX = Canvas->ClipX;
			float ClipY = Canvas->ClipY;
			float LeftPos = 0;

			ClipX -= 100;
			//ClipY = ClipY * 0.60;
			LeftPos = ClipX * 0.3f;
			float TopPos = ClipY * 0.4f;

			int32 X = (int32)LeftPos;
			int32 Y = (int32)TopPos;

			FString Str, StatusStr;
			// First row
// 			Str = FString::Printf(TEXT("TimeWarp: %s"), (FrameSettings->Flags.bTimeWarp) ? TEXT("ON") : TEXT("OFF"));
// 			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
// 
// 			Y += RowHeight;

			//Str = FString::Printf(TEXT("VSync: %s"), (FrameSettings->Flags.bVSync) ? TEXT("ON") : TEXT("OFF"));
			//Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
			//Y += RowHeight;

			Str = FString::Printf(TEXT("Upd on GT/RT: %s / %s"), (!FrameSettings->Flags.bDoNotUpdateOnGT) ? TEXT("ON") : TEXT("OFF"),
				(FrameSettings->Flags.bUpdateOnRT) ? TEXT("ON") : TEXT("OFF"));
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

			Y += RowHeight;

// 			static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
// 			int finFr = CFinishFrameVar->GetInt();
// 			Str = FString::Printf(TEXT("FinFr: %s"), (finFr || FrameSettings->Flags.bTimeWarp) ? TEXT("ON") : TEXT("OFF"));
// 			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
// 
// 			Y += RowHeight;

			Str = FString::Printf(TEXT("PD: %.2f"), FrameSettings->PixelDensity);
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
			Y += RowHeight;

			Str = FString::Printf(TEXT("QueueAhead: %s"), (FrameSettings->QueueAheadStatus == FSettings::EQA_Enabled) ? TEXT("ON") : 
				((FrameSettings->QueueAheadStatus == FSettings::EQA_Default) ? TEXT("DEFLT") : TEXT("OFF")));
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
			Y += RowHeight;

			Str = FString::Printf(TEXT("FOV V/H: %.2f / %.2f deg"), 
				FMath::RadiansToDegrees(FrameSettings->VFOVInRadians), FMath::RadiansToDegrees(FrameSettings->HFOVInRadians));
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

			Y += RowHeight;
			Str = FString::Printf(TEXT("W-to-m scale: %.2f uu/m"), frame->WorldToMetersScale);
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

			//if ((FrameSettings->SupportedHmdCaps & ovrHmdCap_DynamicPrediction) != 0)
			{
				float latencies[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
				const int numOfEntries = sizeof(latencies) / sizeof(latencies[0]);
				if (ovr_GetFloatArray(OvrSession, "DK2Latency", latencies, numOfEntries) == numOfEntries)
				{
					Y += RowHeight;

					char buf[numOfEntries][20];
					char destStr[100];

					FCStringAnsi::Snprintf(destStr, sizeof(destStr), "Latency, ren: %s tw: %s pp: %s err: %s %s",
						FormatLatencyReading(buf[0], sizeof(buf[0]), latencies[0]),
						FormatLatencyReading(buf[1], sizeof(buf[1]), latencies[1]),
						FormatLatencyReading(buf[2], sizeof(buf[2]), latencies[2]),
						FormatLatencyReading(buf[3], sizeof(buf[3]), latencies[3]),
						FormatLatencyReading(buf[4], sizeof(buf[4]), latencies[4]));

					Str = ANSI_TO_TCHAR(destStr);
					Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
				}
			}

			// Second row
			X = (int32)LeftPos + 200;
			Y = (int32)TopPos;

			StatusStr = ((FrameSettings->SupportedTrackingCaps & ovrTrackingCap_Position) != 0) ?
				((FrameSettings->Flags.bHmdPosTracking) ? TEXT("ON") : TEXT("OFF")) : TEXT("UNSUP");
			Str = FString::Printf(TEXT("PosTr: %s"), *StatusStr);
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
			Y += RowHeight;

			Str = FString::Printf(TEXT("Vision: %s"), (frame->Flags.bHaveVisionTracking) ? TEXT("ACQ") : TEXT("LOST"));
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
			Y += RowHeight;

			Str = FString::Printf(TEXT("IPD: %.2f mm"), FrameSettings->InterpupillaryDistance*1000.f);
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
			Y += RowHeight;

// 			StatusStr = ((FrameSettings->SupportedHmdCaps & ovrHmdCap_LowPersistence) != 0) ?
// 				((FrameSettings->Flags.bLowPersistenceMode) ? TEXT("ON") : TEXT("OFF")) : TEXT("UNSUP");
// 			Str = FString::Printf(TEXT("LowPers: %s"), *StatusStr);
// 			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
// 			Y += RowHeight;

// 			StatusStr = ((FrameSettings->SupportedDistortionCaps & ovrDistortionCap_Overdrive) != 0) ?
// 				((FrameSettings->Flags.bOverdrive) ? TEXT("ON") : TEXT("OFF")) : TEXT("UNSUP");
// 			Str = FString::Printf(TEXT("Overdrive: %s"), *StatusStr);
// 			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
// 			Y += RowHeight;
		}

		//TODO:  Where can I get context!?
		UWorld* MyWorld = GWorld;
		if (Canvas && Canvas->SceneView && FrameSettings->Flags.bDrawTrackingCameraFrustum)
		{
			DrawDebugTrackingCameraFrustum(MyWorld, Canvas->SceneView->ViewRotation, Canvas->SceneView->ViewLocation);
		}

		if (Canvas && Canvas->SceneView)
		{
			DrawSeaOfCubes(MyWorld, Canvas->SceneView->ViewLocation);
		}
	}

#endif // #if !UE_BUILD_SHIPPING
}

void FOculusRiftHMD::UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& InViewport, SViewport* ViewportWidget)
{
	check(IsInGameThread());

	if (GIsEditor && ViewportWidget)
	{
		// In editor we are going to check if the viewport widget supports stereo rendering or not.
		if (!ViewportWidget->IsStereoRenderingAllowed())
		{
			return;
		}
	}

	FRHIViewport* const ViewportRHI = InViewport.GetViewportRHI().GetReference();

	TSharedPtr<SWindow> Window = CachedWindow.Pin();
	if (ViewportWidget)
	{
		TSharedPtr<SWidget> CurrentlyCachedWidget = CachedViewportWidget.Pin();
		TSharedRef<SWidget> Widget = ViewportWidget->AsShared();
		if (!Window.IsValid() || Widget != CurrentlyCachedWidget)
		{
			FWidgetPath WidgetPath;
			Window = FSlateApplication::Get().FindWidgetWindow(Widget, WidgetPath);

			CachedViewportWidget = Widget;
			CachedWindow = Window;
		}
	}
	if (!Settings->IsStereoEnabled())
	{
		if ((!bUseSeparateRenderTarget || GIsEditor) && ViewportRHI)
		{
			ViewportRHI->SetCustomPresent(nullptr);
		}
		// Restore AutoResizeViewport mode for the window
		if (ViewportWidget && !IsFullscreenAllowed() && Settings->MirrorWindowSize.X != 0 && Settings->MirrorWindowSize.Y != 0)
		{
			if (Window.IsValid())
			{
				Window->SetViewportSizeDrivenByWindow(true);
			}
		}
		return;
	}

	FGameFrame* CurrentFrame = GetFrame();
	if (!bUseSeparateRenderTarget || !CurrentFrame)
		return;

	check(CurrentFrame);

	CurrentFrame->ViewportSize = InViewport.GetSizeXY();
	CurrentFrame->WindowSize = (Window.IsValid()) ? Window->GetSizeInScreen() : CurrentFrame->ViewportSize;

	check(pCustomPresent);

	pCustomPresent->UpdateViewport(InViewport, ViewportRHI, CurrentFrame);
}

void FOculusRiftHMD::ShutdownRendering()
{
	check(IsInRenderingThread());
	if (pCustomPresent.GetReference())
	{
		pCustomPresent->Shutdown();
		pCustomPresent = NULL;
	}
}

//-------------------------------------------------------------------------------------------------
// FCustomPresent
//-------------------------------------------------------------------------------------------------

FCustomPresent::FCustomPresent()
	: FRHICustomPresent(nullptr)
	, OvrSession(nullptr)
	, LayerMgr(this)
	, MirrorTexture(nullptr)
	, NeedToKillHmd(0)
	, bInitialized(false)
	, bNeedReAllocateTextureSet(true)
	, bNeedReAllocateMirrorTexture(true)
{
	// grab a pointer to the renderer module for displaying our mirror window
	static const FName RendererModuleName("Renderer");
	RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
}

void FCustomPresent::SetRenderContext(FHMDViewExtension* InRenderContext)
{
	if (InRenderContext)
	{
		RenderContext = StaticCastSharedRef<FViewExtension>(InRenderContext->AsShared());
	}
	else
	{
		RenderContext.Reset();
	}
}

void FCustomPresent::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI, FGameFrame* InRenderFrame)
{
	check(IsInGameThread());
	check(InViewportRHI);

	this->ViewportRHI = InViewportRHI;
	ViewportRHI->SetCustomPresent(this);
}

void FCustomPresent::MarkTexturesInvalid()
{
	if (IsInRenderingThread())
	{
		UE_CLOG(!bNeedReAllocateMirrorTexture, LogHMD, Log, TEXT("MarkTexturesInvalid() is called"));
		bNeedReAllocateTextureSet = bNeedReAllocateMirrorTexture = true;
	}
	else if (IsInGameThread())
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(FlushRendering,
			FCustomPresent*, Bridge, this,
			{
				Bridge->MarkTexturesInvalid();
			});
		// Wait for all resources to be released
		FlushRenderingCommands();
	}
}

void FCustomPresent::OnBackBufferResize()
{
	// if we are in the middle of rendering: prevent from calling EndFrame
	if (RenderContext.IsValid())
	{
		RenderContext->bFrameBegun = false;
	}
}

bool FCustomPresent::Present(int32& SyncInterval)
{
	check(IsInRenderingThread());

	if (!RenderContext.IsValid())
	{
		return true; // use regular Present; this frame is not ready yet
	}

	SyncInterval = 0; // turn off VSync for the 'normal Present'.
	bool bHostPresent = RenderContext->GetFrameSettings()->Flags.bMirrorToWindow;

	FinishRendering();
	return bHostPresent;
}

void FCustomPresent::UpdateLayers(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	if (RenderContext.IsValid())
	{
		if (RenderContext->ShowFlags.Rendering)
		{
			check(RenderContext->GetRenderFrame());

			LayerMgr.PreSubmitUpdate_RenderThread(RHICmdList, RenderContext->GetRenderFrame());
		}
	}
}

bool FCustomPresent::AllocateRenderTargetTexture(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	check(SizeX != 0 && SizeY != 0);

	if (NeedToKillHmd)
	{
		return false;
	}

	// it is possible that eye layer is not added to RenderLayers yet, or previously allocated textureSet is not transferred there yet.
	const FHMDLayerDesc* pEyeLayerDesc = LayerMgr.GetLayerDesc(0);
	check(pEyeLayerDesc);
	auto TextureSet = pEyeLayerDesc->GetTextureSet();
	if (!TextureSet.IsValid())
	{
		// grab texture set from the RenderLayers; this should be fine, since RT is suspended.
		check(GIsRenderingThreadSuspended);
		const FHMDRenderLayer* pEyeLayer = LayerMgr.GetRenderLayer_RenderThread_NoLock(0);
		if (pEyeLayer)
		{
			TextureSet = pEyeLayer->GetLayerDesc().GetTextureSet();
		}
	}

	if (!bNeedReAllocateTextureSet && (!TextureSet.IsValid() || (TextureSet->GetSizeX() != SizeX || TextureSet->GetSizeY() != SizeY)))
	{
		UE_LOG(LogHMD, Log, TEXT("Need to reallocate eye buffer, reason: %s"), (TextureSet.IsValid()) ? TEXT("size changed") : TEXT("no textureset was allocated"));
		bNeedReAllocateTextureSet = true;
	}
	else if (TextureSet.IsValid())
	{
		OutTargetableTexture = TextureSet->GetRHITexture2D();
		OutShaderResourceTexture = TextureSet->GetRHITexture2D();
		return true;
	}

	if (OvrSession && bNeedReAllocateTextureSet)
	{
		FTexture2DSetProxyRef ColorTextureSet = CreateTextureSet(SizeX, SizeY, EPixelFormat(Format), NumMips, TexCreate_RenderTargetable | TexCreate_ShaderResource);
		if (ColorTextureSet.IsValid())
		{
			// update the eye layer textureset. at the moment only one eye layer is supported
			const FHMDLayerDesc* pEyeLayerDesc = LayerMgr.GetLayerDesc(0);
			check(pEyeLayerDesc);
			FHMDLayerDesc EyeLayerDesc = *pEyeLayerDesc;
			EyeLayerDesc.SetTextureSet(ColorTextureSet);
			LayerMgr.UpdateLayer(EyeLayerDesc);

			bNeedReAllocateTextureSet = false;
			bNeedReAllocateMirrorTexture = true;

			OutTargetableTexture = ColorTextureSet->GetRHITexture2D();
			OutShaderResourceTexture = ColorTextureSet->GetRHITexture2D();
			return true;
		}
	}
	return false;
}

void FCustomPresent::FinishRendering()
{
	SCOPE_CYCLE_COUNTER(STAT_FinishRendering);

	check(IsInRenderingThread());

	check(RenderContext.IsValid());

	if (RenderContext->bFrameBegun)
	{
		ovrResult res = LayerMgr.SubmitFrame(RenderContext.Get());
		if (!OVR_SUCCESS(res))
		{
			if (res == ovrError_DisplayLost || res == ovrError_NoHmd)
			{
				UE_CLOG(!bNeedReAllocateTextureSet, LogHMD, Log, TEXT("DisplayLost, mark texturesets invalid...."));
				bNeedReAllocateMirrorTexture = bNeedReAllocateTextureSet = true;
				FPlatformAtomics::InterlockedExchange(&NeedToKillHmd, 1);
			}
		}

		// Update frame stats
#if STATS
		struct
		{
			float LatencyRender;
			float LatencyTimewarp;
			float LatencyPostPresent;
			float ErrorRender;
			float ErrorTimewarp;
		} DK2Latency;

		const unsigned int DK2LatencyCount = sizeof(DK2Latency) / sizeof(float);

		if (ovr_GetFloatArray(RenderContext->OvrSession, "DK2Latency", (float*)&DK2Latency, DK2LatencyCount) == DK2LatencyCount)
		{
			SET_FLOAT_STAT(STAT_LatencyRender, DK2Latency.LatencyRender * 1000.0f);
			SET_FLOAT_STAT(STAT_LatencyTimewarp, DK2Latency.LatencyTimewarp * 1000.0f);
			SET_FLOAT_STAT(STAT_LatencyPostPresent, DK2Latency.LatencyPostPresent * 1000.0f);
			SET_FLOAT_STAT(STAT_ErrorRender, DK2Latency.ErrorRender * 1000.0f);
			SET_FLOAT_STAT(STAT_ErrorTimewarp, DK2Latency.ErrorTimewarp * 1000.0f);
		}
#endif
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("Skipping frame: FinishRendering called with no corresponding BeginRendering (was BackBuffer re-allocated?)"));
	}
	RenderContext->bFrameBegun = false;
	SetRenderContext(nullptr);
}

#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
#endif //!PLATFORM_MAC
