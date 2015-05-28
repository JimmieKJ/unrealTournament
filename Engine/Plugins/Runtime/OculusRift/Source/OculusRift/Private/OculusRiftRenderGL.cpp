// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
//
#include "HMDPrivatePCH.h"
#include "OculusRiftHMD.h"

#if !PLATFORM_MAC
#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#if defined(OVR_GL)
#include "OVR_CAPI_GL.h"

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"
#include "ScreenRendering.h"

#include "SlateBasics.h"

//////////////////////////////////////////////////////////////////////////
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
	FViewExtension* const RenderContext = GetRenderContext();

	if (RenderContext->bFrameBegun)
	{
		// Finish the frame and let OVR do buffer swap (Present) and flush/sync.
		const ovrTexture eyeTextures[2] = { EyeTexture[0].Texture, EyeTexture[1].Texture };
		ovrHmd_SubmitFrameLegacy(RenderContext->Hmd, RenderContext->RenderFrame->FrameNumber, RenderContext->CurEyeRenderPose, eyeTextures); // This function will present
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

bool FOculusRiftHMD::OGLBridge::Present(int SyncInterval)
{
	check(IsInRenderingThread());

	FinishRendering();

	return false; // indicates that we are presenting here, UE shouldn't do Present.
}
#endif // #if defined(OVR_GL)

#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
#endif //#if !PLATFORM_MAC
