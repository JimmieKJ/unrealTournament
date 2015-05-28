// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
//
#include "HMDPrivatePCH.h"
#include "OculusRiftHMD.h"

// Mac uses 0.5.0 SDK, while PC - 0.6.0. This version supports 0.5.0 (Mac/Linux).
#if PLATFORM_MAC
#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"

#include "SlateBasics.h"

#ifndef OVR_SDK_RENDERING
void FSettings::FDistortionMesh::Reset()
{
	delete[] pVertices;
	delete[] pIndices;
	pVertices = NULL;
	pIndices = NULL;
	NumVertices = NumIndices = NumTriangles = 0;
}
#endif

void FOculusRiftHMD::PrecalculateDistortionMesh()
{
#ifndef OVR_SDK_RENDERING
	check(IsInGameThread());

	FSettings* CurrentSettings = GetSettings(); // we need global settings here rather than frame settings
	check(CurrentSettings);

	for (unsigned eyeNum = 0; eyeNum < 2; ++eyeNum)
	{
		// Allocate & generate distortion mesh vertices.
		ovrDistortionMesh meshData;

		if (!ovrHmd_CreateDistortionMesh(Hmd, CurrentSettings->EyeRenderDesc[eyeNum].Eye, CurrentSettings->EyeRenderDesc[eyeNum].Fov, CurrentSettings->DistortionCaps, &meshData))
		{
			check(false);
			continue;
		}

		// alloc the data
		ovrDistortionVertex* ovrMeshVertData = new ovrDistortionVertex[meshData.VertexCount];

		// Convert to final vertex data.
		FDistortionVertex *pVerts = new FDistortionVertex[meshData.VertexCount];
		FDistortionVertex *pCurVert = pVerts;
		ovrDistortionVertex* pCurOvrVert = meshData.pVertexData;

		for (unsigned vertNum = 0; vertNum < meshData.VertexCount; ++vertNum)
		{
			pCurVert->Position.X = pCurOvrVert->ScreenPosNDC.x;
			pCurVert->Position.Y = pCurOvrVert->ScreenPosNDC.y;
			pCurVert->TexR = FVector2D(pCurOvrVert->TanEyeAnglesR.x, pCurOvrVert->TanEyeAnglesR.y);
			pCurVert->TexG = FVector2D(pCurOvrVert->TanEyeAnglesG.x, pCurOvrVert->TanEyeAnglesG.y);
			pCurVert->TexB = FVector2D(pCurOvrVert->TanEyeAnglesB.x, pCurOvrVert->TanEyeAnglesB.y);
			pCurVert->VignetteFactor = pCurOvrVert->VignetteFactor;
			pCurVert->TimewarpFactor = pCurOvrVert->TimeWarpFactor;
			pCurOvrVert++;
			pCurVert++;
		}

		CurrentSettings->pDistortionMesh[eyeNum] = MakeShareable(new FSettings::FDistortionMesh());
		CurrentSettings->pDistortionMesh[eyeNum]->NumTriangles = meshData.IndexCount / 3; 
		CurrentSettings->pDistortionMesh[eyeNum]->NumIndices = meshData.IndexCount;
		CurrentSettings->pDistortionMesh[eyeNum]->NumVertices = meshData.VertexCount;
		CurrentSettings->pDistortionMesh[eyeNum]->pVertices = pVerts;

		check(sizeof(*meshData.pIndexData) == sizeof(uint16));
		CurrentSettings->pDistortionMesh[eyeNum]->pIndices = new uint16[meshData.IndexCount];
		FMemory::Memcpy(CurrentSettings->pDistortionMesh[eyeNum]->pIndices, meshData.pIndexData, sizeof(uint16)*meshData.IndexCount);

		ovrHmd_DestroyDistortionMesh(&meshData);
	}
#endif
}

void FOculusRiftHMD::DrawDistortionMesh_RenderThread(FRenderingCompositePassContext& Context, const FIntPoint& InTextureSize)
{
	check(IsInRenderingThread());

	const FSceneView& View = Context.View;
	if (View.StereoPass == eSSP_FULL)
	{
		return;
	}

#ifndef OVR_SDK_RENDERING
	float ClipSpaceQuadZ = 0.0f;
	FMatrix QuadTexTransform = FMatrix::Identity;
	FMatrix QuadPosTransform = FMatrix::Identity;
	const FIntRect SrcRect = View.ViewRect;

	auto RenderFrame = GetRenderFrameFromContext();
	check(RenderFrame);
	auto RenderFrameSettings = RenderFrame->GetSettings();
	check(RenderFrameSettings);

	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	int ViewportSizeX = ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeX();
	int ViewportSizeY = ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeY();
	RHICmdList.SetViewport(0, 0, 0.0f, ViewportSizeX, ViewportSizeY, 1.0f);

	TSharedPtr<FSettings::FDistortionMesh> mesh = RenderFrameSettings->pDistortionMesh[(View.StereoPass == eSSP_LEFT_EYE) ? 0 : 1];

	DrawIndexedPrimitiveUP(Context.RHICmdList, PT_TriangleList, 0, mesh->NumVertices, mesh->NumTriangles, mesh->pIndices,
		sizeof(mesh->pIndices[0]), mesh->pVertices, sizeof(mesh->pVertices[0]));
#else
	check(0);
#endif
}

void FOculusRiftHMD::GetEyeRenderParams_RenderThread(const FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	check(IsInRenderingThread());
#ifndef OVR_SDK_RENDERING
	auto RenderFrame = GetRenderFrameFromContext();
	check(RenderFrame);
	auto RenderFrameSettings = RenderFrame->GetSettings();
	check(RenderFrameSettings);

	const unsigned eyeIdx = (Context.View.StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
	EyeToSrcUVOffsetValue.X = RenderFrameSettings->UVScaleOffset[eyeIdx][1].x;
	EyeToSrcUVOffsetValue.Y = RenderFrameSettings->UVScaleOffset[eyeIdx][1].y;

	EyeToSrcUVScaleValue.X = RenderFrameSettings->UVScaleOffset[eyeIdx][0].x;
	EyeToSrcUVScaleValue.Y = RenderFrameSettings->UVScaleOffset[eyeIdx][0].y;
#else
	check(0);
#endif
}

void FOculusRiftHMD::GetTimewarpMatrices_RenderThread(const FRenderingCompositePassContext& Context, FMatrix& EyeRotationStart, FMatrix& EyeRotationEnd) const
{
	check(IsInRenderingThread());
#ifndef OVR_SDK_RENDERING
	check(RenderContext.IsValid());

	const ovrEyeType eye = (Context.View.StereoPass == eSSP_LEFT_EYE) ? ovrEye_Left : ovrEye_Right;
	ovrMatrix4f timeWarpMatrices[2];
	if (RenderContext->bFrameBegun)
	{
		ovrHmd_GetEyeTimewarpMatrices(Hmd, eye, RenderContext->CurEyeRenderPose[eye], timeWarpMatrices);
	}
	EyeRotationStart = ToFMatrix(timeWarpMatrices[0]);
	EyeRotationEnd = ToFMatrix(timeWarpMatrices[1]);
#else
	check(0);
#endif
}

void FViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());
	FViewExtension& RenderContext = *this;
	FGameFrame* CurrentFrame = static_cast<FGameFrame*>(RenderContext.RenderFrame.Get());

	if (bFrameBegun || !CurrentFrame || !CurrentFrame->Settings->IsStereoEnabled())
	{
		return;
	}
	FSettings* FrameSettings = CurrentFrame->GetSettings();
	RenderContext.ShowFlags = ViewFamily.EngineShowFlags;

	RenderContext.CurHeadPose = CurrentFrame->HeadPose;

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

#ifdef OVR_SDK_RENDERING 
	check(ViewFamily.RenderTarget->GetRenderTargetTexture());
	CurrentFrame->GetSettings()->SetEyeRenderViewport(ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeX()/2, 
													  ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeY());
	pPresentBridge->BeginRendering(RenderContext, ViewFamily.RenderTarget->GetRenderTargetTexture());

	ovrHmd_BeginFrame(Hmd, RenderContext.RenderFrame->FrameNumber);
#else
	FOculusRiftHMD* OculusPlugin = static_cast<FOculusRiftHMD*>(Delegate);
	TSharedPtr<FHMDViewExtension, ESPMode::ThreadSafe> SharedPtr(AsShared());
	OculusPlugin->RenderContext = StaticCastSharedPtr<FViewExtension>(SharedPtr);

	const FIntPoint TextureSize = FrameSettings->GetTextureSize();
	for (size_t eyeNum = 0; eyeNum < 2; ++eyeNum)
	{
		ovrHmd_GetRenderScaleAndOffset(FrameSettings->EyeRenderDesc[eyeNum].Fov,
			OVR::Sizei(TextureSize.X, TextureSize.Y),
			ToOVRRecti(FrameSettings->EyeRenderViewport[eyeNum]),
			FrameSettings->UVScaleOffset[eyeNum]);
	}

	ovrHmd_BeginFrameTiming(Hmd, CurrentFrame->FrameNumber);
#endif
	RenderContext.bFrameBegun = true;

	if (RenderContext.ShowFlags.Rendering)
	{
		// get latest orientation/position and cache it
		ovrTrackingState ts;
		const ovrVector3f hmdToEyeViewOffset[2] =
		{
			FrameSettings->EyeRenderDesc[0].HmdToEyeViewOffset,
			FrameSettings->EyeRenderDesc[1].HmdToEyeViewOffset
		};
		ovrPosef EyeRenderPose[2];
		ovrHmd_GetEyePoses(Hmd, CurrentFrame->FrameNumber, hmdToEyeViewOffset, EyeRenderPose, &ts);

		// Take new EyeRenderPose is bUpdateOnRT.
		// if !bOrientationChanged && !bPositionChanged then we still need to use new eye pose (for timewarp)
		if (FrameSettings->Flags.bUpdateOnRT ||
			(!CurrentFrame->Flags.bOrientationChanged && !CurrentFrame->Flags.bPositionChanged))
		{
			RenderContext.CurHeadPose = ts.HeadPose.ThePose;
			FMemory::Memcpy(RenderContext.CurEyeRenderPose, EyeRenderPose);
		}
		else
		{
			FMemory::Memcpy<ovrPosef[2]>(RenderContext.CurEyeRenderPose, CurrentFrame->EyeRenderPose);
		}
	}
}

void FViewExtension::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View)
{
	check(IsInRenderingThread());
	FViewExtension& RenderContext = *this; //static_cast<FViewExtension&>(InRenderContext);
	const FGameFrame* CurrentFrame = static_cast<const FGameFrame*>(RenderContext.RenderFrame.Get());

	if (!RenderContext.ShowFlags.Rendering || !CurrentFrame || !CurrentFrame->Settings->IsStereoEnabled())
	{
		return;
	}

	if (RenderContext.ShowFlags.Rendering && CurrentFrame->Settings->Flags.bUpdateOnRT)
	{
		const ovrEyeType eyeIdx = (View.StereoPass == eSSP_LEFT_EYE) ? ovrEye_Left : ovrEye_Right;
		FQuat	CurrentEyeOrientation;
		FVector	CurrentEyePosition;
		CurrentFrame->PoseToOrientationAndPosition(RenderContext.CurEyeRenderPose[eyeIdx], CurrentEyeOrientation, CurrentEyePosition);

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
			CurrentFrame->PoseToOrientationAndPosition(RenderContext.CurHeadPose, HeadOrientation, View.BaseHmdLocation);
		}

		// The HMDPosition already has HMD orientation applied.
		// Apply rotational difference between HMD orientation and ViewRotation
		// to HMDPosition vector. 
		// PositionOffset should be already applied to View.ViewLocation on GT in PlayerCameraUpdate.
		const FVector DeltaPosition = CurrentEyePosition - View.BaseHmdLocation;
		const FVector vEyePosition = DeltaControlOrientation.RotateVector(DeltaPosition);
		View.ViewLocation += vEyePosition;

		//UE_LOG(LogHMD, Log, TEXT("VDLTPOS: %.3f %.3f %.3f"), vEyePosition.X, vEyePosition.Y, vEyePosition.Z);

		if (CurrentFrame->Flags.bOrientationChanged || CurrentFrame->Flags.bPositionChanged)
		{
			View.UpdateViewMatrix();
		}
	}
}

#ifdef OVR_SDK_RENDERING 
FCustomPresent* FOculusRiftHMD::GetActiveRHIBridgeImpl() const
{
#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)
	if (pD3D11Bridge)
	{
		return pD3D11Bridge;
	}
#endif
#if defined(OVR_GL)
	if (pOGLBridge)
	{
		return pOGLBridge;
	}
#endif
	return nullptr;
}

void FOculusRiftHMD::CalculateRenderTargetSize(const FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	check(IsInGameThread());

	if (!Settings->IsStereoEnabled())
	{
		return;
	}

	if (Viewport.GetClient()->GetEngineShowFlags()->ScreenPercentage)
	{
		float value = GetActualScreenPercentage();
		if (value > 0.0f)
		{
			InOutSizeX = FMath::CeilToInt(InOutSizeX * value / 100.f) + GetSettings()->GetTexturePaddingPerEye()*2;
			InOutSizeY = FMath::CeilToInt(InOutSizeY * value / 100.f);
		}
	}
}

bool FOculusRiftHMD::NeedReAllocateViewportRenderTarget(const FViewport& Viewport)
{
	check(IsInGameThread());
	if (IsStereoEnabled())
	{
		const uint32 InSizeX = Viewport.GetSizeXY().X;
		const uint32 InSizeY = Viewport.GetSizeXY().Y;
		const FIntPoint RenderTargetSize = Viewport.GetRenderTargetTextureSizeXY();

		uint32 NewSizeX = InSizeX, NewSizeY = InSizeY;
		CalculateRenderTargetSize(Viewport, NewSizeX, NewSizeY);
		if (NewSizeX != RenderTargetSize.X || NewSizeY != RenderTargetSize.Y)
		{
			return true;
		}
	}
	return false;
}

#else // no direct rendering

FGameFrame* FOculusRiftHMD::GetRenderFrameFromContext() const
{
	check(IsInRenderingThread());

	check(RenderContext.IsValid());
	return static_cast<FGameFrame*>(RenderContext->RenderFrame.Get());
}

void FOculusRiftHMD::FinishRenderingFrame_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	check(RenderContext.IsValid());
	auto RenderFrame = GetRenderFrameFromContext();
	check(RenderFrame);
	auto RenderFrameSettings = RenderFrame->GetSettings();
	check(RenderFrameSettings);
	if (RenderContext->bFrameBegun)
	{
		if (RenderFrameSettings->Flags.bTimeWarp)
		{
			RHICmdList.BlockUntilGPUIdle();
		}
		ovrHmd_EndFrameTiming(Hmd);
		RenderContext->bFrameBegun = false;
		RenderContext.Reset();
	}
}

#endif // #ifdef OVR_SDK_RENDERING 

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
			Str = FString::Printf(TEXT("TimeWarp: %s"), (FrameSettings->Flags.bTimeWarp) ? TEXT("ON") : TEXT("OFF"));
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

			Y += RowHeight;
			Str = FString::Printf(TEXT("VSync: %s"), (FrameSettings->Flags.bVSync) ? TEXT("ON") : TEXT("OFF"));
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
		
			Y += RowHeight;
			Str = FString::Printf(TEXT("Upd on GT/RT: %s / %s"), (!FrameSettings->Flags.bDoNotUpdateOnGT) ? TEXT("ON") : TEXT("OFF"),
				(FrameSettings->Flags.bUpdateOnRT) ? TEXT("ON") : TEXT("OFF"));
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

			Y += RowHeight;
			static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
			int finFr = CFinishFrameVar->GetInt();
			Str = FString::Printf(TEXT("FinFr: %s"), (finFr || FrameSettings->Flags.bTimeWarp) ? TEXT("ON") : TEXT("OFF"));
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

			Y += RowHeight;
			int32 sp = (int32)GetActualScreenPercentage();
			Str = FString::Printf(TEXT("SP: %d"), sp);
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

			Y += RowHeight;
			Str = FString::Printf(TEXT("FOV V/H: %.2f / %.2f deg"), 
				FMath::RadiansToDegrees(FrameSettings->VFOVInRadians), FMath::RadiansToDegrees(FrameSettings->HFOVInRadians));
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

			Y += RowHeight;
			Str = FString::Printf(TEXT("W-to-m scale: %.2f uu/m"), frame->WorldToMetersScale);
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

			if ((FrameSettings->SupportedHmdCaps & ovrHmdCap_DynamicPrediction) != 0)
			{
				float latencies[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
				const int numOfEntries = sizeof(latencies) / sizeof(latencies[0]);
				if (ovrHmd_GetFloatArray(Hmd, "DK2Latency", latencies, numOfEntries) == numOfEntries)
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

			StatusStr = ((FrameSettings->SupportedHmdCaps & ovrHmdCap_LowPersistence) != 0) ?
				((FrameSettings->Flags.bLowPersistenceMode) ? TEXT("ON") : TEXT("OFF")) : TEXT("UNSUP");
			Str = FString::Printf(TEXT("LowPers: %s"), *StatusStr);
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
			Y += RowHeight;

			StatusStr = ((FrameSettings->SupportedDistortionCaps & ovrDistortionCap_Overdrive) != 0) ?
				((true) ? TEXT("ON") : TEXT("OFF")) : TEXT("UNSUP");
			Str = FString::Printf(TEXT("Overdrive: %s"), *StatusStr);
			Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
			Y += RowHeight;
		}

		//TODO:  Where can I get context!?
		UWorld* MyWorld = GWorld;
		if (FrameSettings->Flags.bDrawTrackingCameraFrustum)
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

	if (!IsStereoEnabled())
	{
		if ((!bUseSeparateRenderTarget || GIsEditor) && ViewportRHI)
		{
			ViewportRHI->SetCustomPresent(nullptr);
		}
#if PLATFORM_WINDOWS
		if (OSWindowHandle)
		{
			ovrHmd_AttachToWindow(Hmd, NULL, NULL, NULL);
			OSWindowHandle = nullptr;

			// Restore AutoResizeViewport mode for the window
			if (ViewportWidget && !IsFullscreenAllowed() && Settings->MirrorWindowSize.X != 0 && Settings->MirrorWindowSize.Y != 0)
			{
				FWidgetPath WidgetPath;
				TSharedRef<SWidget> Widget = ViewportWidget->AsShared();
				TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Widget, WidgetPath);
				if (Window.IsValid())
				{
					Window->SetViewportSizeDrivenByWindow(true);
				}
			}
		}
#endif
		return;
	}

#if PLATFORM_WINDOWS
	if (ViewportRHI)
	{
		void *wnd = ViewportRHI->GetNativeWindow();
		if (wnd && wnd != OSWindowHandle)
		{
			OSWindowHandle = wnd;
			if (!ovrHmd_AttachToWindow(Hmd, OSWindowHandle, NULL, NULL))
			{
				UE_LOG(LogHMD, Error, TEXT("ovrHmd_AttachToWindow failed."));
			}
		}
	}
#endif

	if (!bUseSeparateRenderTarget)
		return;

	FGameFrame* CurrentFrame = GetFrame();
	check(CurrentFrame);

	CurrentFrame->ViewportSize = InViewport.GetSizeXY();
	//CurrentFrame->ViewportRHI  = ViewportRHI;

#ifdef OVR_SDK_RENDERING

	check(GetActiveRHIBridgeImpl());

	GetActiveRHIBridgeImpl()->UpdateViewport(InViewport, ViewportRHI, CurrentFrame);
#endif // #ifdef OVR_SDK_RENDERING
}

#ifdef OVR_SDK_RENDERING
void FOculusRiftHMD::ShutdownRendering()
{
	check(IsInRenderingThread());
#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)
	if (pD3D11Bridge)
	{
		pD3D11Bridge->Shutdown();
		pD3D11Bridge = NULL;
	}
#endif
#if defined(OVR_GL)
	if (pOGLBridge)
	{
		pOGLBridge->Shutdown();
		pOGLBridge = NULL;
	}
#endif
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

void FCustomPresent::UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI, FGameFrame* InRenderFrame)
{
	check(IsInGameThread());
	check(ViewportRHI);

	this->ViewportRHI = ViewportRHI;
	ViewportRHI->SetCustomPresent(this);
}

#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)
// FIXME: in Direct mode we must hold DXGIAdapter to avoid a crash at the exit because of shimming.
// It does cause D3D resources to leak. But it is better than a crash, right?
// Should be already fixed in the next Oculus SDK version.
static void AddRefAdapter()
{
	if (FOculusRiftHMD::bDirectModeHack)
	{
		ID3D11Device* D3DDevice = (ID3D11Device*)RHIGetNativeDevice();
		if (D3DDevice)
		{
			D3DDevice->AddRef();
			IDXGIDevice * pDXGIDevice;
			HRESULT hr = D3DDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
			if (hr == S_OK)
			{
				IDXGIAdapter* pAdapter;
				pDXGIDevice->GetAdapter(&pAdapter);
				if (pAdapter)
				{
					UE_LOG(LogHMD, Log, TEXT("++ Addrefing Adapter %p!"), pAdapter);
					pAdapter->AddRef();
				}
			}
		}
	}
}

FOculusRiftHMD::D3D11Bridge::D3D11Bridge()
	: FCustomPresent()
{
	FMemory::Memset(Cfg, 0);
	FMemory::Memset(EyeTexture, 0);

	if (FOculusRiftHMD::bDirectModeHack)
	{
		// FIXME: in Direct mode we must hold DXGIAdapter to avoid a crash at the exit because of shimming.
		// It does cause D3D resources to leak. But it is better than a crash, right?
		// Should be already fixed in the next Oculus SDK version.
		ENQUEUE_UNIQUE_RENDER_COMMAND(AdapterAddRef,
		{
			AddRefAdapter();
		});
	}
}

void FOculusRiftHMD::D3D11Bridge::Shutdown()
{
	check(IsInRenderingThread());
	if (FOculusRiftHMD::bDirectModeHack)
	{
		AddRefAdapter();
	}
	Reset();
}

void FOculusRiftHMD::D3D11Bridge::BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT)
{
	check(IsInRenderingThread());

	SetRenderContext(&InRenderContext);

	ID3D11Device* D3DDevice = (ID3D11Device*)RHIGetNativeDevice();
	ID3D11DeviceContext* D3DDeviceContext = nullptr;
	if (D3DDevice)
	{
		D3DDevice->GetImmediateContext(&D3DDeviceContext);
	}
	if (!bInitialized || D3DDevice != Cfg.D3D11.pDevice || D3DDeviceContext != Cfg.D3D11.pDeviceContext)
	{
		if (D3DDevice && D3DDevice != Cfg.D3D11.pDevice)
		{
			if (FOculusRiftHMD::bDirectModeHack)
			{
				// FIXME: in Direct mode we must hold D3DDevice to avoid a crash at the exit because of shimming.
				// It does cause D3D resources to leak. But it is better than a crash, right?
				// Should be already fixed in the next Oculus SDK version.
				D3DDevice->AddRef();
			}
		}

		Cfg.D3D11.Header.API = ovrRenderAPI_D3D11;
		Cfg.D3D11.Header.Multisample = 0;
		// Note, neither Device nor Context are AddRef-ed here. Not sure, if we need to.
		Cfg.D3D11.pDevice = D3DDevice;
		Cfg.D3D11.pDeviceContext = D3DDeviceContext;
		bNeedReinitRendererAPI = true;
		bInitialized = true;
	}

	if (bInitialized)
	{
		ID3D11RenderTargetView* const	pD3DBBRT = (ID3D11RenderTargetView*)ViewportRHI->GetNativeBackBufferRT();
		IDXGISwapChain*         const	pD3DSC = (IDXGISwapChain*)ViewportRHI->GetNativeSwapChain();
		check(IsValidRef(RT));
		ID3D11Texture2D* const 			pD3DRT = (ID3D11Texture2D*)RT->GetNativeResource();
		ID3D11ShaderResourceView* const pD3DSRV = (ID3D11ShaderResourceView*)RT->GetNativeShaderResourceView();
		const uint32 RTSizeX = RT->GetSizeX();
		const uint32 RTSizeY = RT->GetSizeY();

		FGameFrame* CurrentFrame = GetRenderFrame();
		check(CurrentFrame);
		FSettings* FrameSettings = CurrentFrame->GetSettings();
		check(FrameSettings);

		if (Cfg.D3D11.pBackBufferRT != pD3DBBRT ||
			Cfg.D3D11.pSwapChain != pD3DSC ||
			Cfg.D3D11.Header.BackBufferSize.w != CurrentFrame->ViewportSize.X ||
			Cfg.D3D11.Header.BackBufferSize.h != CurrentFrame->ViewportSize.Y)
		{
			// Note, neither BackBufferRT nor SwapChain are AddRef-ed here. Not sure, if we need to.
			// If yes, then them should be released in ReleaseBackBuffer().
			Cfg.D3D11.pBackBufferRT = pD3DBBRT;
			Cfg.D3D11.pSwapChain = pD3DSC;
			Cfg.D3D11.Header.BackBufferSize.w = CurrentFrame->ViewportSize.X;
			Cfg.D3D11.Header.BackBufferSize.h = CurrentFrame->ViewportSize.Y;
			bNeedReinitRendererAPI = true;
		}

		if (EyeTexture[0].D3D11.pTexture != pD3DRT || EyeTexture[0].D3D11.pSRView != pD3DSRV ||
			EyeTexture[0].D3D11.Header.TextureSize.w != RTSizeX || EyeTexture[0].D3D11.Header.TextureSize.h != RTSizeY ||
			((OVR::Recti)EyeTexture[0].D3D11.Header.RenderViewport) != ToOVRRecti(FrameSettings->EyeRenderViewport[0]))
		{
			for (int eye = 0; eye < 2; ++eye)
			{
				ovrD3D11TextureData oldEye = EyeTexture[eye].D3D11;
				EyeTexture[eye].D3D11.Header.API = ovrRenderAPI_D3D11;
				EyeTexture[eye].D3D11.Header.TextureSize = OVR::Sizei(RTSizeX, RTSizeY);
				EyeTexture[eye].D3D11.Header.RenderViewport = ToOVRRecti(FrameSettings->EyeRenderViewport[eye]);
				EyeTexture[eye].D3D11.pTexture = pD3DRT;
				EyeTexture[eye].D3D11.pSRView = pD3DSRV;
				if (EyeTexture[eye].D3D11.pTexture)
				{
					EyeTexture[eye].D3D11.pTexture->AddRef();
				}
				if (EyeTexture[eye].D3D11.pSRView)
				{
					EyeTexture[eye].D3D11.pSRView->AddRef();
				}

				if (oldEye.pTexture)
				{
					oldEye.pTexture->Release();
				}
				if (oldEye.pSRView)
				{
					oldEye.pSRView->Release();
				}
			}
		}

		if (bNeedReinitRendererAPI)
		{
			check(Cfg.D3D11.pSwapChain); // make sure Config is initialized

			if (!ovrHmd_ConfigureRendering(RenderContext->Hmd, &Cfg.Config, FrameSettings->DistortionCaps,
				FrameSettings->EyeFov, FrameSettings->EyeRenderDesc))
			{
				UE_LOG(LogHMD, Warning, TEXT("D3D11 ovrHmd_ConfigureRenderAPI failed."));
				return;
			}
			bNeedReinitRendererAPI = false;
		}
	}
}

void FOculusRiftHMD::D3D11Bridge::FinishRendering()
{
	check(IsInRenderingThread());
	
	check(RenderContext.IsValid());

	if (RenderContext->bFrameBegun)
	{
		// Finish the frame and let OVR do buffer swap (Present) and flush/sync.
		const ovrTexture eyeTextures[2] = { EyeTexture[0].Texture, EyeTexture[1].Texture };
		ovrHmd_EndFrame(RenderContext->Hmd, RenderContext->CurEyeRenderPose, eyeTextures); // This function will present
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("Skipping frame: FinishRendering called with no corresponding BeginRendering (was BackBuffer re-allocated?)"));
	}
	RenderContext->bFrameBegun = false;
	SetRenderContext(nullptr);
}

void FOculusRiftHMD::D3D11Bridge::Reset_RenderThread()
{
	Cfg.D3D11.pDevice = nullptr;
	Cfg.D3D11.pDeviceContext = nullptr;

	for (int eye = 0; eye < 2; ++eye)
	{
		if (EyeTexture[eye].D3D11.pTexture)
		{
			EyeTexture[eye].D3D11.pTexture->Release();
			EyeTexture[eye].D3D11.pTexture = nullptr;
		}
		if (EyeTexture[eye].D3D11.pSRView)
		{
			EyeTexture[eye].D3D11.pSRView->Release();
			EyeTexture[eye].D3D11.pSRView = nullptr;
		}
	}

	Cfg.D3D11.pBackBufferRT = nullptr;
	Cfg.D3D11.pSwapChain = nullptr;

	bNeedReinitRendererAPI = false;

	if (RenderContext.IsValid())
	{
		RenderContext->bFrameBegun = false;
		SetRenderContext(nullptr);
	}
}

void FOculusRiftHMD::D3D11Bridge::Reset()
{
	if (IsInGameThread())
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(ResetD3D,
		FOculusRiftHMD::D3D11Bridge*, Bridge, this,
		{
			Bridge->Reset_RenderThread();
		});
		// Wait for all resources to be released
		FlushRenderingCommands();
	}
	else
	{
		Reset_RenderThread();
	}

	bInitialized = false;
}

void FOculusRiftHMD::D3D11Bridge::OnBackBufferResize()
{
	Cfg.D3D11.pBackBufferRT		= nullptr;
	Cfg.D3D11.pSwapChain		= nullptr;
 
 	bNeedReinitRendererAPI = true;

	// if we are in the middle of rendering: prevent from calling EndFrame
	if (RenderContext.IsValid())
	{
		RenderContext->bFrameBegun = false;
	}
}

bool FOculusRiftHMD::D3D11Bridge::Present(int& SyncInterval)
{
	check(IsInRenderingThread());

	if (!RenderContext.IsValid())
	{
		return true; // use regular Present; this frame is not ready yet
	}

	FinishRendering();

	return false; // indicates that we are presenting here, UE shouldn't do Present.
}
#endif // #if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)

//////////////////////////////////////////////////////////////////////////
#if defined(OVR_GL)
FOculusRiftHMD::OGLBridge::OGLBridge() : 
	FCustomPresent()
{
	FMemory::Memset(Cfg, 0);
	FMemory::Memset(EyeTexture, 0);
}

void FOculusRiftHMD::OGLBridge::BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT)
{
	SetRenderContext(&InRenderContext);

	if (!bInitialized)
	{
		Cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
		Cfg.OGL.Header.Multisample = 1;
		bNeedReinitRendererAPI = true;
		bInitialized = true;
	}
	if (bInitialized)
	{
		FGameFrame* CurrentFrame = GetRenderFrame();
		check(CurrentFrame);
		FSettings* FrameSettings = CurrentFrame->GetSettings();
		check(FrameSettings);

		bool bWinChanged = false;
#if PLATFORM_WINDOWS
		HWND Window = *(HWND*)ViewportRHI->GetNativeWindow();
		bWinChanged = (Cfg.OGL.Window != Window);
#elif PLATFORM_MAC
		//@TODO
		//NSWindow
#elif PLATFORM_LINUX
		//@TODO
#endif
		if (bWinChanged ||
			Cfg.OGL.Header.BackBufferSize.w != CurrentFrame->ViewportSize.X ||
			Cfg.OGL.Header.BackBufferSize.h != CurrentFrame->ViewportSize.Y)
		{
			Cfg.OGL.Header.BackBufferSize = OVR::Sizei(CurrentFrame->ViewportSize.X, CurrentFrame->ViewportSize.Y);
#if PLATFORM_WINDOWS
			Cfg.OGL.Window = Window;
#elif PLATFORM_MAC
			//@TODO
#elif PLATFORM_LINUX
			//@TODO
			//	Cfg.OGL.Disp = AddParams; //?
			//	Cfg.OGL.Win = hWnd; //?
#endif
			bNeedReinitRendererAPI = true;
		}

		check(IsValidRef(RT));
		const uint32 RTSizeX = RT->GetSizeX();
		const uint32 RTSizeY = RT->GetSizeY();
		GLuint RTTexId = *(GLuint*)RT->GetNativeResource();

		if ((EyeTexture[0].OGL.TexId != RTTexId ||
			EyeTexture[0].OGL.Header.TextureSize.w != RTSizeX || EyeTexture[0].OGL.Header.TextureSize.h != RTSizeY))
		{
			EyeTexture[0].OGL.Header.API = ovrRenderAPI_OpenGL;
			EyeTexture[0].OGL.Header.TextureSize = OVR::Sizei(RTSizeX, RTSizeY);
			EyeTexture[0].OGL.Header.RenderViewport = ToOVRRecti(FrameSettings->EyeRenderViewport[0]);
			EyeTexture[0].OGL.TexId = RTTexId;

			// Right eye uses the same texture, but different rendering viewport.
			EyeTexture[1] = EyeTexture[0];
			EyeTexture[1].OGL.Header.RenderViewport = ToOVRRecti(FrameSettings->EyeRenderViewport[1]);
		}
		if (bNeedReinitRendererAPI)
		{
			if (!ovrHmd_ConfigureRendering(RenderContext->Hmd, &Cfg.Config, FrameSettings->DistortionCaps,
				FrameSettings->EyeFov, FrameSettings->EyeRenderDesc))
			{
				UE_LOG(LogHMD, Warning, TEXT("OGL ovrHmd_ConfigureRenderAPI failed."));
				return;
			}
			bNeedReinitRendererAPI = false;
		}
	}
}

void FOculusRiftHMD::OGLBridge::FinishRendering()
{
	check(IsInRenderingThread());

	check(RenderContext.IsValid());

	if (RenderContext->bFrameBegun)
	{
		// Finish the frame and let OVR do buffer swap (Present) and flush/sync.
		const ovrTexture eyeTextures[2] = { EyeTexture[0].Texture, EyeTexture[1].Texture };
		ovrHmd_EndFrame(RenderContext->Hmd, RenderContext->CurEyeRenderPose, eyeTextures); // This function will present
		RenderContext->bFrameBegun = false;
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("Skipping frame: FinishRendering called with no corresponding BeginRendering (was BackBuffer re-allocated?)"));
	}
	SetRenderContext(nullptr);
}

void FOculusRiftHMD::OGLBridge::Reset()
{
	check(IsInRenderingThread());

	EyeTexture[0].OGL.TexId = 0;
	EyeTexture[1].OGL.TexId = 0;

	bNeedReinitRendererAPI = false;
	bInitialized = false;

	if (RenderContext.IsValid())
	{
		RenderContext->bFrameBegun = false;
		SetRenderContext(nullptr);
	}
}

void FOculusRiftHMD::OGLBridge::OnBackBufferResize()
{
	bNeedReinitRendererAPI = true;

	// if we are in the middle of rendering: prevent from calling EndFrame
	if (RenderContext.IsValid())
	{
		RenderContext->bFrameBegun = false;
	}
}

bool FOculusRiftHMD::OGLBridge::Present(int& SyncInterval)
{
	check(IsInRenderingThread());

	FinishRendering();

	return false; // indicates that we are presenting here, UE shouldn't do Present.
}
#endif // #if defined(OVR_GL)
#endif // OVR_SDK_RENDERING


#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
#endif //#if PLATFORM_MAC
