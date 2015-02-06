// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
//
#include "OculusRiftPrivate.h"
#include "OculusRiftHMD.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#include "../Src/OVR_Stereo.h"

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"

#include "SlateBasics.h"

#ifndef OVR_SDK_RENDERING
void FOculusRiftHMD::FDistortionMesh::Clear()
{
	delete[] pVertices;
	delete[] pIndices;
	pVertices = NULL;
	pIndices = NULL;
	NumVertices = NumIndices = NumTriangles = 0;
}
#endif

FOculusRiftHMD::FRenderParams::FRenderParams(FOculusRiftHMD* plugin)
	: 
	  FrameNumber(~0u)
	, bFrameBegun(false)
	, bTimeWarp(false)
	, ShowFlags(ESFIM_All0)
{
	#ifndef OVR_SDK_RENDERING
	FMemory::Memset(UVScale, 0, sizeof(UVScale));
	FMemory::Memset(UVOffset, 0, sizeof(UVOffset));
	#endif
}

void FOculusRiftHMD::PrecalculatePostProcess_NoLock()
{
#ifndef OVR_SDK_RENDERING
	for (unsigned eyeNum = 0; eyeNum < 2; ++eyeNum)
	{
		// Allocate & generate distortion mesh vertices.
		ovrDistortionMesh meshData;

		if (!ovrHmd_CreateDistortionMesh(Hmd, EyeRenderDesc[eyeNum].Eye, EyeRenderDesc[eyeNum].Fov, DistortionCaps, &meshData))
		{
			check(false);
			continue;
		}
		ovrHmd_GetRenderScaleAndOffset(EyeRenderDesc[eyeNum].Fov,
			TextureSize, EyeRenderViewport[eyeNum],
			(ovrVector2f*) UVScaleOffset[eyeNum]);

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

		pDistortionMesh[eyeNum] = *new FDistortionMesh();
		pDistortionMesh[eyeNum]->NumTriangles = meshData.IndexCount / 3; 
		pDistortionMesh[eyeNum]->NumIndices = meshData.IndexCount;
		pDistortionMesh[eyeNum]->NumVertices = meshData.VertexCount;
		pDistortionMesh[eyeNum]->pVertices = pVerts;

		check(sizeof(*meshData.pIndexData) == sizeof(uint16));
		pDistortionMesh[eyeNum]->pIndices = new uint16[meshData.IndexCount];
		FMemory::Memcpy(pDistortionMesh[eyeNum]->pIndices, meshData.pIndexData, sizeof(uint16)*meshData.IndexCount);

		ovrHmd_DestroyDistortionMesh(&meshData);
	}
#endif
}

void FOculusRiftHMD::DrawDistortionMesh_RenderThread(FRenderingCompositePassContext& Context, const FSceneView& View, const FIntPoint& TextureSize)
{
#ifndef OVR_SDK_RENDERING
	float ClipSpaceQuadZ = 0.0f;
	FMatrix QuadTexTransform = FMatrix::Identity;
	FMatrix QuadPosTransform = FMatrix::Identity;
	const FIntRect SrcRect = View.ViewRect;

	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	int ViewportSizeX = ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeX();
	int ViewportSizeY = ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeY();
	RHICmdList.SetViewport(0, 0, 0.0f, ViewportSizeX, ViewportSizeY, 1.0f);

	Ptr<FDistortionMesh> mesh = RenderParams_RenderThread.pDistortionMesh[(View.StereoPass == eSSP_LEFT_EYE) ? 0 : 1];

	DrawIndexedPrimitiveUP(Context.RHICmdList, PT_TriangleList, 0, mesh->NumVertices, mesh->NumTriangles, mesh->pIndices,
		sizeof(mesh->pIndices[0]), mesh->pVertices, sizeof(mesh->pVertices[0]));
#else
	check(0);
#endif
}

void FOculusRiftHMD::GetEyeRenderParams_RenderThread(EStereoscopicPass StereoPass, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	check(IsInRenderingThread());
#ifndef OVR_SDK_RENDERING
	const unsigned eyeIdx = (StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
	EyeToSrcUVOffsetValue.X = RenderParams_RenderThread.UVOffset[eyeIdx].x;
	EyeToSrcUVOffsetValue.Y = RenderParams_RenderThread.UVOffset[eyeIdx].y;

	EyeToSrcUVScaleValue.X = RenderParams_RenderThread.UVScale[eyeIdx].x;
	EyeToSrcUVScaleValue.Y = RenderParams_RenderThread.UVScale[eyeIdx].y;
#else
	check(0);
#endif
}

void FOculusRiftHMD::GetTimewarpMatrices_RenderThread(EStereoscopicPass StereoPass, FMatrix& EyeRotationStart, FMatrix& EyeRotationEnd) const
{
	check(IsInRenderingThread());
#ifndef OVR_SDK_RENDERING
	const ovrEyeType eye = (StereoPass == eSSP_LEFT_EYE) ? ovrEye_Left : ovrEye_Right;
	ovrMatrix4f timeWarpMatrices[2];
	if (RenderParams_RenderThread.bFrameBegun)
	{
		ovrHmd_GetEyeTimewarpMatrices(Hmd, eye, RenderParams_RenderThread.EyeRenderPose[eye], timeWarpMatrices);
	}
	EyeRotationStart = ToFMatrix(timeWarpMatrices[0]);
	EyeRotationEnd = ToFMatrix(timeWarpMatrices[1]);
#else
	check(0);
#endif
}

void FOculusRiftHMD::PreRenderViewFamily_RenderThread(FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());

	RenderParams_RenderThread.ShowFlags = ViewFamily.EngineShowFlags;
	// FrameNumber comes from SceneRenderer.FrameNumber and contains already incremented
	// GFrameNumber. Therefore, decrementing it to match with the frame number used on a game thread.
	RenderParams_RenderThread.FrameNumber = ViewFamily.FrameNumber - 1;

#ifndef OVR_SDK_RENDERING 
	{
		// make a copy of StereoParams to access from the RenderThread.
		Lock::Locker lock(&StereoParamsLock);
		RenderParams_RenderThread.EyeRenderDesc[0] = EyeRenderDesc[0];
		RenderParams_RenderThread.EyeRenderDesc[1] = EyeRenderDesc[1];
		RenderParams_RenderThread.UVScale[0] = UVScaleOffset[0][0];
		RenderParams_RenderThread.UVScale[1] = UVScaleOffset[1][0];
		RenderParams_RenderThread.UVOffset[0] = UVScaleOffset[0][1];
		RenderParams_RenderThread.UVOffset[1] = UVScaleOffset[1][1];
		RenderParams_RenderThread.pDistortionMesh[0] = pDistortionMesh[0];
		RenderParams_RenderThread.pDistortionMesh[1] = pDistortionMesh[1];
		RenderParams_RenderThread.bTimeWarp = Flags.bTimeWarp;
		RenderParams_RenderThread.DeltaControlOrientation = DeltaControlOrientation;
	}
#else
	{
		// make a copy of StereoParams to access from the RenderThread.
		Lock::Locker lock(&StereoParamsLock);
		RenderParams_RenderThread.EyeRenderDesc[0] = EyeRenderDesc[0];
		RenderParams_RenderThread.EyeRenderDesc[1] = EyeRenderDesc[1];
		RenderParams_RenderThread.EyeFov[0] = EyeFov[0];
		RenderParams_RenderThread.EyeFov[1] = EyeFov[1];
		RenderParams_RenderThread.bTimeWarp = Flags.bTimeWarp;
		RenderParams_RenderThread.DeltaControlOrientation = DeltaControlOrientation;
	}
#endif

	if (RenderParams_RenderThread.ShowFlags.Rendering)
	{
		// get latest orientation/position and cache it
		ovrTrackingState ts;
		ovrVector3f hmdToEyeViewOffset[2] =
		{
			RenderParams_RenderThread.EyeRenderDesc[0].HmdToEyeViewOffset,
			RenderParams_RenderThread.EyeRenderDesc[1].HmdToEyeViewOffset
		};
		ovrHmd_GetEyePoses(Hmd, RenderParams_RenderThread.FrameNumber, hmdToEyeViewOffset, RenderParams_RenderThread.EyeRenderPose, &ts);
	}

	BeginRendering_RenderThread();
}

void FOculusRiftHMD::PreRenderView_RenderThread(FSceneView& View)
{
	check(IsInRenderingThread());

	if (!RenderParams_RenderThread.ShowFlags.Rendering)
	{
		return;
	}

	const ovrEyeType eyeIdx = (View.StereoPass == eSSP_LEFT_EYE) ? ovrEye_Left : ovrEye_Right;
	FQuat	CurrentEyeOrientation;
	FVector	CurrentEyePosition;

	PoseToOrientationAndPosition(RenderParams_RenderThread.EyeRenderPose[eyeIdx], CurrentEyeOrientation, CurrentEyePosition);

	if (Flags.bUpdateOnRT)
	{
		// Apply updated orientation to corresponding View at recalc matrices.
		// The updated position will be applied from inside of the UpdateViewMatrix() call.
		const FQuat DeltaOrient = View.BaseHmdOrientation.Inverse() * CurrentEyeOrientation;
		View.ViewRotation = FRotator(View.ViewRotation.Quaternion() * DeltaOrient);

		View.UpdateViewMatrix();
	}
}

void FOculusRiftHMD::BeginRendering_RenderThread()
{
	check(IsInRenderingThread());
	if (RenderParams_RenderThread.bFrameBegun)
	{
		return;
	}

#ifdef OVR_SDK_RENDERING 
	{
		Lock::Locker lock(&StereoParamsLock);

		GetActiveRHIBridgeImpl()->BeginRendering();
	}

	ovrHmd_BeginFrame(Hmd, RenderParams_RenderThread.FrameNumber);
#else
	ovrHmd_BeginFrameTiming(Hmd, RenderParams_RenderThread.FrameNumber);
#endif
	RenderParams_RenderThread.bFrameBegun = true;
}

#ifdef OVR_SDK_RENDERING 
FOculusRiftHMD::BridgeBaseImpl* FOculusRiftHMD::GetActiveRHIBridgeImpl()
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

void FOculusRiftHMD::CalculateRenderTargetSize(uint32& InOutSizeX, uint32& InOutSizeY) const
{
	check(IsInGameThread());

	if (Flags.bScreenPercentageEnabled)
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"));
		float value = CVar->GetValueOnGameThread();
		if (value > 0.0f)
		{
			InOutSizeX = FMath::CeilToInt(InOutSizeX * value / 100.f);
			InOutSizeY = FMath::CeilToInt(InOutSizeY * value / 100.f);
		}
	}
}

bool FOculusRiftHMD::NeedReAllocateViewportRenderTarget(const FViewport& Viewport) const
{
	check(IsInGameThread());
	if (IsStereoEnabled())
	{
		const uint32 InSizeX = Viewport.GetSizeXY().X;
		const uint32 InSizeY = Viewport.GetSizeXY().Y;
		FIntPoint RenderTargetSize;
		RenderTargetSize.X = Viewport.GetRenderTargetTexture()->GetSizeX();
		RenderTargetSize.Y = Viewport.GetRenderTargetTexture()->GetSizeY();

		uint32 NewSizeX = InSizeX, NewSizeY = InSizeY;
		CalculateRenderTargetSize(NewSizeX, NewSizeY);
		if (NewSizeX != RenderTargetSize.X || NewSizeY != RenderTargetSize.Y)
		{
			return true;
		}
	}
	return false;
}

#else // no direct rendering

void FOculusRiftHMD::FinishRenderingFrame_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	if (RenderParams_RenderThread.bFrameBegun)
	{
		check(IsInRenderingThread());
		if (RenderParams_RenderThread.bTimeWarp)
		{
			RHICmdList.BlockUntilGPUIdle();
		}
		ovrHmd_EndFrameTiming(Hmd);
		RenderParams_RenderThread.bFrameBegun = false;
	}
}

#endif // #ifdef OVR_SDK_RENDERING 

static const char* FormatLatencyReading(char* buff, UPInt size, float val)
{
	if (val < 0.000001f)
	{
		OVR_strcpy(buff, size, "N/A   ");
	}
	else
	{
		OVR_sprintf(buff, size, "%4.2fms", val * 1000.0f);
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

void FOculusRiftHMD::DrawDebug(UCanvas* Canvas, EStereoscopicPass StereoPass)
{
#if !UE_BUILD_SHIPPING
	check(IsInGameThread());
	if (StereoPass == eSSP_FULL)
	{
		if (Flags.bDrawGrid)
		{
			bool bPopTransform = false;
			if (EyeRenderDesc[0].DistortedViewport.Size.w != FMath::CeilToInt(Canvas->ClipX / 2) ||
				EyeRenderDesc[0].DistortedViewport.Size.h != Canvas->ClipY)
			{
				// scale if resolution of the Canvas does not match the viewport
				bPopTransform = true;
				Canvas->Canvas->PushAbsoluteTransform(FScaleMatrix(
					FVector((Canvas->ClipX * 0.5f) / float(EyeRenderDesc[0].DistortedViewport.Size.w),
					Canvas->ClipY / float(EyeRenderDesc[0].DistortedViewport.Size.h),
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

				int renderViewportX = EyeRenderDesc[eye].DistortedViewport.Pos.x;
				int renderViewportY = EyeRenderDesc[eye].DistortedViewport.Pos.y;
				int renderViewportW = EyeRenderDesc[eye].DistortedViewport.Size.w;
				int renderViewportH = EyeRenderDesc[eye].DistortedViewport.Size.h;

				lineStep = 48;
				Vector2f rendertargetNDC = OVR::FovPort(EyeRenderDesc[eye].Fov).TanAngleToRendertargetNDC(Vector2f(0.0f));
				midX = (int)((rendertargetNDC.x * 0.5f + 0.5f) * (float)renderViewportW + 0.5f);
				midY = (int)((rendertargetNDC.y * 0.5f + 0.5f) * (float)renderViewportH + 0.5f);
				limitX = Alg::Max(renderViewportW - midX, midX);
				limitY = Alg::Max(renderViewportH - midY, midY);

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
		}
		return;
	}
	else if (IsStereoEnabled() && Flags.bShowStats)
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
		Str = FString::Printf(TEXT("TimeWarp: %s"), (Flags.bTimeWarp) ? TEXT("ON") : TEXT("OFF"));
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

		Y += RowHeight;
		Str = FString::Printf(TEXT("VSync: %s"), (Flags.bVSync) ? TEXT("ON") : TEXT("OFF"));
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
		
		Y += RowHeight;
		Str = FString::Printf(TEXT("Upd on GT/RT: %s / %s"), (!Flags.bDoNotUpdateOnGT) ? TEXT("ON") : TEXT("OFF"), 
			(Flags.bUpdateOnRT) ? TEXT("ON") : TEXT("OFF"));
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

		Y += RowHeight;
		static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
		int finFr = CFinishFrameVar->GetInt();
		Str = FString::Printf(TEXT("FinFr: %s"), (finFr || Flags.bTimeWarp) ? TEXT("ON") : TEXT("OFF"));
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

		Y += RowHeight;
		static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
		int32 sp = (int32)CScrPercVar->GetFloat();
		Str = FString::Printf(TEXT("SP: %d"), sp);
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

		Y += RowHeight;
		Str = FString::Printf(TEXT("FOV V/H: %.2f / %.2f deg"), FMath::RadiansToDegrees(VFOVInRadians), FMath::RadiansToDegrees(HFOVInRadians));
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

		Y += RowHeight;
		Str = FString::Printf(TEXT("W-to-m scale: %.2f uu/m"), WorldToMetersScale);
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

		if ((SupportedHmdCaps & ovrHmdCap_DynamicPrediction) != 0)
		{
			float latencies[3] = { 0.0f, 0.0f, 0.0f };
			if (ovrHmd_GetFloatArray(Hmd, "DK2Latency", latencies, 3) == 3)
			{
				Y += RowHeight;

				char buf[3][20];
				char destStr[100];

				OVR_sprintf(destStr, sizeof(destStr), "Latency, ren: %s tw: %s pp: %s",
					FormatLatencyReading(buf[0], sizeof(buf[0]), latencies[0]),
					FormatLatencyReading(buf[1], sizeof(buf[1]), latencies[1]),
					FormatLatencyReading(buf[2], sizeof(buf[2]), latencies[2]));

				Str = ANSI_TO_TCHAR(destStr);
				Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
			}
		}

		// Second row
		X = (int32)LeftPos + 200;
		Y = (int32)TopPos;

		StatusStr = ((SupportedTrackingCaps & ovrTrackingCap_Position) != 0) ?
			((Flags.bHmdPosTracking) ? TEXT("ON") : TEXT("OFF")) : TEXT("UNSUP");
		Str = FString::Printf(TEXT("PosTr: %s"), *StatusStr);
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
		Y += RowHeight;

		Str = FString::Printf(TEXT("Vision: %s"), (Flags.bHaveVisionTracking) ? TEXT("ACQ") : TEXT("LOST"));
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
		Y += RowHeight;

		Str = FString::Printf(TEXT("IPD: %.2f mm"), InterpupillaryDistance*1000.f);
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
		Y += RowHeight;

		StatusStr = ((SupportedHmdCaps & ovrHmdCap_LowPersistence) != 0) ? 
			((Flags.bLowPersistenceMode) ? TEXT("ON") : TEXT("OFF")) : TEXT("UNSUP");
		Str = FString::Printf(TEXT("LowPers: %s"), *StatusStr);
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
		Y += RowHeight;

		StatusStr = ((SupportedDistortionCaps & ovrDistortionCap_Overdrive) != 0) ?
			((Flags.bOverdrive) ? TEXT("ON") : TEXT("OFF")) : TEXT("UNSUP");
		Str = FString::Printf(TEXT("Overdrive: %s"), *StatusStr);
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
		Y += RowHeight;
	}
#endif // #if !UE_BUILD_SHIPPING
}

void FOculusRiftHMD::UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& InViewport, SViewport* ViewportWidget)
{
	check(IsInGameThread());

	if (GIsEditor)
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
			if (ViewportWidget && !IsFullscreenAllowed() && MirrorWindowSize.X != 0 && MirrorWindowSize.Y != 0)
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
			ovrHmd_AttachToWindow(Hmd, OSWindowHandle, NULL, NULL);
		}
	}
#endif

#ifdef OVR_SDK_RENDERING

	check(GetActiveRHIBridgeImpl());

	const FTexture2DRHIRef& RT = InViewport.GetRenderTargetTexture();
	check(IsValidRef(RT));
	const FIntPoint NewEyeRTSize = FIntPoint((RT->GetSizeX() + 1) / 2, RT->GetSizeY());
	if (EyeViewportSize != NewEyeRTSize)
	{
		EyeViewportSize.X = NewEyeRTSize.X;
		EyeViewportSize.Y = NewEyeRTSize.Y;
		Flags.bNeedUpdateStereoRenderingParams = true;
	}
	if (Flags.bNeedUpdateStereoRenderingParams)
	{
		UpdateStereoRenderingParams();
	}
	GetActiveRHIBridgeImpl()->UpdateViewport(InViewport, ViewportRHI);
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

#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)
FOculusRiftHMD::D3D11Bridge::D3D11Bridge(FOculusRiftHMD* plugin):
	BridgeBaseImpl(plugin), 
	bNeedReinitEyeTextures(false)
{
	memset(&Cfg, 0, sizeof(Cfg));
	memset(&EyeTexture, 0, sizeof(EyeTexture));
	memset(&EyeTexture_RenderThread, 0, sizeof(EyeTexture_RenderThread));
}

void FOculusRiftHMD::D3D11Bridge::BeginRendering()
{
	check(IsInRenderingThread());

	ID3D11Device* D3DDevice = (ID3D11Device*)RHIGetNativeDevice();
	ID3D11DeviceContext* D3DDeviceContext = nullptr;
	if (D3DDevice)
	{
		D3DDevice->GetImmediateContext(&D3DDeviceContext);
	}
	if (!bInitialized || D3DDevice != Cfg.D3D11.pDevice || D3DDeviceContext != Cfg.D3D11.pDeviceContext)
	{
		OVR::Lock::Locker lock(&ModifyLock);
		Cfg.D3D11.Header.API = ovrRenderAPI_D3D11;
		Cfg.D3D11.Header.Multisample = 1; //?? RenderParams.Multisample;
		// Note, neither Device nor Context are AddRef-ed here. Not sure, if we need to.
		Cfg.D3D11.pDevice = D3DDevice;
		Cfg.D3D11.pDeviceContext = D3DDeviceContext;
		bNeedReinitRendererAPI = true;
		bInitialized = true;
	}

	if (bInitialized)
	{
		if (bNeedReinitRendererAPI)
		{
			OVR::Lock::Locker lock(&ModifyLock);
			check(Cfg.D3D11.pSwapChain); // make sure Config is initialized
			Plugin->UpdateDistortionCaps();
			if (!ovrHmd_ConfigureRendering(Plugin->Hmd, &Cfg.Config, Plugin->DistortionCaps, 
				Plugin->RenderParams_RenderThread.EyeFov, Plugin->RenderParams_RenderThread.EyeRenderDesc))
			{
				UE_LOG(LogHMD, Warning, TEXT("D3D11 ovrHmd_ConfigureRenderAPI failed."));
				return;
			}
			bNeedReinitRendererAPI = false;
		}

		UpdateEyeTextures();
	}
}

void FOculusRiftHMD::D3D11Bridge::UpdateEyeTextures()
{
	if (bNeedReinitEyeTextures)
	{
		OVR::Lock::Locker lock(&ModifyEyeTexturesLock);

		ovrD3D11TextureData oldEye0 = EyeTexture_RenderThread[0].D3D11;
		ovrD3D11TextureData oldEye1 = EyeTexture_RenderThread[1].D3D11;

		EyeTexture_RenderThread[0] = EyeTexture[0];
		if (EyeTexture_RenderThread[0].D3D11.pTexture)
		{
			EyeTexture_RenderThread[0].D3D11.pTexture->AddRef();
		}
		if (EyeTexture_RenderThread[0].D3D11.pSRView)
		{
			EyeTexture_RenderThread[0].D3D11.pSRView->AddRef();
		}
		EyeTexture_RenderThread[1] = EyeTexture[1];
		if (EyeTexture_RenderThread[1].D3D11.pTexture)
		{
			EyeTexture_RenderThread[1].D3D11.pTexture->AddRef();
		}
		if (EyeTexture_RenderThread[1].D3D11.pSRView)
		{
			EyeTexture_RenderThread[1].D3D11.pSRView->AddRef();
		}

		if (oldEye0.pTexture)
		{
			oldEye0.pTexture->Release();
		}
		if (oldEye1.pTexture)
		{
			oldEye1.pTexture->Release();
		}
		if (oldEye0.pSRView)
		{
			oldEye0.pSRView->Release();
		}
		if (oldEye1.pSRView)
		{
			oldEye1.pSRView->Release();
		}

		bNeedReinitEyeTextures = false;
	}
}

void FOculusRiftHMD::D3D11Bridge::FinishRendering()
{
	check(IsInRenderingThread());

	UpdateEyeTextures();

	if (Plugin->RenderParams_RenderThread.bFrameBegun)
	{
		// Finish the frame and let OVR do buffer swap (Present) and flush/sync.
		const ovrTexture eyeTextures[2] = { EyeTexture_RenderThread[0].Texture, EyeTexture_RenderThread[1].Texture };
		ovrHmd_EndFrame(Plugin->Hmd, Plugin->RenderParams_RenderThread.EyeRenderPose, eyeTextures); // This function will present
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("Skipping frame: FinishRendering called with no corresponding BeginRendering (was BackBuffer re-allocated?)"));
	}
	Plugin->RenderParams_RenderThread.bFrameBegun = false;
}

void FOculusRiftHMD::D3D11Bridge::Reset_RenderThread()
{
	Cfg.D3D11.pDevice = nullptr;
	Cfg.D3D11.pDeviceContext = nullptr;

	OVR::Lock::Locker lock(&ModifyEyeTexturesLock);
	if (EyeTexture[0].D3D11.pTexture)
	{
		EyeTexture[0].D3D11.pTexture->Release();
		EyeTexture[0].D3D11.pTexture = (ID3D11Texture2D*)nullptr;
	}
	if (EyeTexture[0].D3D11.pSRView)
	{
		EyeTexture[0].D3D11.pSRView->Release();
		EyeTexture[0].D3D11.pSRView = nullptr;
	}
	if (EyeTexture[1].D3D11.pTexture)
	{
		EyeTexture[1].D3D11.pTexture->Release();
		EyeTexture[1].D3D11.pTexture = nullptr;
	}
	if (EyeTexture[1].D3D11.pSRView)
	{
		EyeTexture[1].D3D11.pSRView->Release();
		EyeTexture[1].D3D11.pSRView = nullptr;
	}

	Cfg.D3D11.pBackBufferRT = nullptr;
	Cfg.D3D11.pSwapChain = nullptr;

	if (EyeTexture_RenderThread[0].D3D11.pTexture)
	{
		EyeTexture_RenderThread[0].D3D11.pTexture->Release();
		EyeTexture_RenderThread[0].D3D11.pTexture = nullptr;
	}
	if (EyeTexture_RenderThread[0].D3D11.pSRView)
	{
		EyeTexture_RenderThread[0].D3D11.pSRView->Release();
		EyeTexture_RenderThread[0].D3D11.pSRView = nullptr;
	}
	if (EyeTexture_RenderThread[1].D3D11.pTexture)
	{
		EyeTexture_RenderThread[1].D3D11.pTexture->Release();
		EyeTexture_RenderThread[1].D3D11.pTexture = nullptr;
	}
	if (EyeTexture_RenderThread[1].D3D11.pSRView)
	{
		EyeTexture_RenderThread[1].D3D11.pSRView->Release();
		EyeTexture_RenderThread[1].D3D11.pSRView = nullptr;
	}
	bNeedReinitEyeTextures = false;
	bNeedReinitRendererAPI = false;
	Plugin->RenderParams_RenderThread.bFrameBegun = false;
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

void FOculusRiftHMD::D3D11Bridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI)
{
	check(IsInGameThread());
	check(ViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	ID3D11RenderTargetView* const	pD3DBBRT = (ID3D11RenderTargetView*)ViewportRHI->GetNativeBackBufferRT();
	IDXGISwapChain*         const	pD3DSC = (IDXGISwapChain*)ViewportRHI->GetNativeSwapChain();
	check(IsValidRef(RT));
	ID3D11Texture2D* const 			pD3DRT = (ID3D11Texture2D*)RT->GetNativeResource();
	ID3D11ShaderResourceView* const pD3DSRV = (ID3D11ShaderResourceView*)RT->GetNativeShaderResourceView();
	const uint32 RTSizeX = RT->GetSizeX();
	const uint32 RTSizeY = RT->GetSizeY();

	if (Cfg.D3D11.pBackBufferRT != pD3DBBRT ||
		Cfg.D3D11.pSwapChain != pD3DSC ||
		Cfg.D3D11.Header.BackBufferSize.w != Viewport.GetSizeXY().X ||
		Cfg.D3D11.Header.BackBufferSize.h != Viewport.GetSizeXY().Y)
	{
		OVR::Lock::Locker lock(&ModifyLock);
		// Note, neither BackBufferRT nor SwapChain are AddRef-ed here. Not sure, if we need to.
		// If yes, then them should be released in ReleaseBackBuffer().
		Cfg.D3D11.pBackBufferRT = pD3DBBRT;
		Cfg.D3D11.pSwapChain = pD3DSC;
		Cfg.D3D11.Header.BackBufferSize.w = Viewport.GetSizeXY().X;
		Cfg.D3D11.Header.BackBufferSize.h = Viewport.GetSizeXY().Y;
		bNeedReinitRendererAPI = true;
		Plugin->RenderParams_RenderThread.bFrameBegun = false;
	}

	if (EyeTexture[0].D3D11.pTexture != pD3DRT || EyeTexture[0].D3D11.pSRView != pD3DSRV ||
		EyeTexture[0].D3D11.Header.TextureSize.w != RTSizeX || EyeTexture[0].D3D11.Header.TextureSize.h != RTSizeY)
	{
		OVR::Lock::Locker lock(&ModifyEyeTexturesLock);

		ovrD3D11TextureData oldEye0 = EyeTexture[0].D3D11;
		EyeTexture[0].D3D11.Header.API = ovrRenderAPI_D3D11;
		EyeTexture[0].D3D11.Header.TextureSize = Sizei(RTSizeX, RTSizeY);
		EyeTexture[0].D3D11.Header.RenderViewport = Plugin->EyeRenderViewport[0];
		EyeTexture[0].D3D11.pTexture = pD3DRT;
		EyeTexture[0].D3D11.pSRView = pD3DSRV;
		if (EyeTexture[0].D3D11.pTexture)
		{
			EyeTexture[0].D3D11.pTexture->AddRef();
		}
		if (EyeTexture[0].D3D11.pSRView)
		{
			EyeTexture[0].D3D11.pSRView->AddRef();
		}

		if (oldEye0.pTexture)
		{
			oldEye0.pTexture->Release();
		}
		if (oldEye0.pSRView)
		{
			oldEye0.pSRView->Release();
		}

		// Right eye uses the same texture, but different rendering viewport.
		ovrD3D11TextureData oldEye1 = EyeTexture[1].D3D11;
		EyeTexture[1] = EyeTexture[0];
		EyeTexture[1].D3D11.Header.RenderViewport = Plugin->EyeRenderViewport[1];
		if (EyeTexture[1].D3D11.pTexture)
		{
			EyeTexture[1].D3D11.pTexture->AddRef();
		}
		if (EyeTexture[1].D3D11.pSRView)
		{
			EyeTexture[1].D3D11.pSRView->AddRef();
		}

		if (oldEye1.pTexture)
		{
			oldEye1.pTexture->Release();
		}
		if (oldEye1.pSRView)
		{
			oldEye1.pSRView->Release();
		}

		bNeedReinitEyeTextures = true;
	}

	this->ViewportRHI = ViewportRHI;
 	ViewportRHI->SetCustomPresent(this);
}


void FOculusRiftHMD::D3D11Bridge::OnBackBufferResize()
{
	Cfg.D3D11.pBackBufferRT		= nullptr;
	Cfg.D3D11.pSwapChain		= nullptr;
 
 	bNeedReinitRendererAPI = true;

	// if we are in the middle of rendering: prevent from calling EndFrame
	Plugin->RenderParams_RenderThread.bFrameBegun = false;
}

bool FOculusRiftHMD::D3D11Bridge::Present(int SyncInterval)
{
	check(IsInRenderingThread());

	FinishRendering();

	return false; // indicates that we are presenting here, UE shouldn't do Present.
}
#endif // #if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)

//////////////////////////////////////////////////////////////////////////
#if defined(OVR_GL)
FOculusRiftHMD::OGLBridge::OGLBridge(FOculusRiftHMD* plugin) : 
	BridgeBaseImpl(plugin), 
	bNeedReinitEyeTextures(false)
{
	memset(&Cfg, 0, sizeof(Cfg));
	memset(&EyeTexture, 0, sizeof(EyeTexture));
	memset(&EyeTexture_RenderThread, 0, sizeof(EyeTexture_RenderThread));
	Init();
}

void FOculusRiftHMD::OGLBridge::BeginRendering()
{
	if (bInitialized)
	{
		if (bNeedReinitRendererAPI)
		{
			OVR::Lock::Locker lock(&ModifyLock);
			Plugin->UpdateDistortionCaps();
			if (!ovrHmd_ConfigureRendering(Plugin->Hmd, &Cfg.Config, Plugin->DistortionCaps, 
				Plugin->RenderParams_RenderThread.EyeFov, Plugin->RenderParams_RenderThread.EyeRenderDesc))
			{
				UE_LOG(LogHMD, Warning, TEXT("OGL ovrHmd_ConfigureRenderAPI failed."));
				return;
			}
			bNeedReinitRendererAPI = false;
		}
		if (bNeedReinitEyeTextures)
		{
			OVR::Lock::Locker lock(&ModifyEyeTexturesLock);

			EyeTexture_RenderThread[0] = EyeTexture[0];
			EyeTexture_RenderThread[1] = EyeTexture[1];
			bNeedReinitEyeTextures = false;
		}
	}
}

void FOculusRiftHMD::OGLBridge::FinishRendering()
{
	check(IsInRenderingThread());

	if (bNeedReinitEyeTextures)
	{
		// make sure we use most recent textures, otherwise there will 
		// be an assertion.
		OVR::Lock::Locker lock(&ModifyEyeTexturesLock);

		EyeTexture_RenderThread[0] = EyeTexture[0];
		EyeTexture_RenderThread[1] = EyeTexture[1];
		bNeedReinitEyeTextures = false;
	}

	if (Plugin->RenderParams_RenderThread.bFrameBegun)
	{
		// Finish the frame and let OVR do buffer swap (Present) and flush/sync.
		const ovrTexture eyeTextures[2] = { EyeTexture_RenderThread[0].Texture, EyeTexture_RenderThread[1].Texture };
		ovrHmd_EndFrame(Plugin->Hmd, Plugin->RenderParams_RenderThread.EyeRenderPose, eyeTextures); // This function will present
		Plugin->RenderParams_RenderThread.bFrameBegun = false;
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("Skipping frame: FinishRendering called with no corresponding BeginRendering (was BackBuffer re-allocated?)"));
	}
}

void FOculusRiftHMD::OGLBridge::Init()
{
	Cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
	Cfg.OGL.Header.Multisample = 1; 
	bNeedReinitRendererAPI = true;
	bInitialized = true;
}

void FOculusRiftHMD::OGLBridge::Reset()
{
	check(IsInRenderingThread());

	EyeTexture[0].OGL.TexId = 0;
	EyeTexture[1].OGL.TexId = 0;
	EyeTexture_RenderThread[0].OGL.TexId = 0;
	EyeTexture_RenderThread[1].OGL.TexId = 0;

	Plugin->RenderParams_RenderThread.bFrameBegun = false;
	bNeedReinitEyeTextures = false;
	bNeedReinitRendererAPI = false;
	bInitialized = false;
}

void FOculusRiftHMD::OGLBridge::OnBackBufferResize()
{
	bNeedReinitRendererAPI = true;

	// if we are in the middle of rendering: prevent from calling EndFrame
	Plugin->RenderParams_RenderThread.bFrameBegun = false;
}

void FOculusRiftHMD::OGLBridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI)
{
	check(IsInGameThread());
	check(ViewportRHI);

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
		Cfg.OGL.Header.BackBufferSize.w != Viewport.GetSizeXY().X ||
		Cfg.OGL.Header.BackBufferSize.h != Viewport.GetSizeXY().Y)
	{
		OVR::Lock::Locker lock(&ModifyLock);

		Cfg.OGL.Header.BackBufferSize = Sizei(Viewport.GetSizeXY().X, Viewport.GetSizeXY().Y);
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

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));
	const uint32 RTSizeX = RT->GetSizeX();
	const uint32 RTSizeY = RT->GetSizeY();
	GLuint RTTexId = *(GLuint*)RT->GetNativeResource();

	if (EyeTexture[0].OGL.TexId != RTTexId ||
		EyeTexture[0].OGL.Header.TextureSize.w != RTSizeX || EyeTexture[0].OGL.Header.TextureSize.h != RTSizeY)
	{
		OVR::Lock::Locker lock(&ModifyEyeTexturesLock);

		EyeTexture[0].OGL.Header.API = ovrRenderAPI_OpenGL;
		EyeTexture[0].OGL.Header.TextureSize = Sizei(RTSizeX, RTSizeY);
		EyeTexture[0].OGL.Header.RenderViewport = Plugin->EyeRenderViewport[0];
		EyeTexture[0].OGL.TexId = RTTexId;

		// Right eye uses the same texture, but different rendering viewport.
		EyeTexture[1] = EyeTexture[0];
		EyeTexture[1].OGL.Header.RenderViewport = Plugin->EyeRenderViewport[1];

		bNeedReinitEyeTextures = true;
	}
	this->ViewportRHI = ViewportRHI;
	ViewportRHI->SetCustomPresent(this);
}

bool FOculusRiftHMD::OGLBridge::Present(int SyncInterval)
{
	check(IsInRenderingThread());

	FinishRendering();

	return false; // indicates that we are presenting here, UE shouldn't do Present.
}
#endif // #if defined(OVR_GL)
#endif // OVR_SDK_RENDERING


#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS