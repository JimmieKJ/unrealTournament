// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "HMDPrivatePCH.h"
#include "OculusRiftHMD.h"

#if !PLATFORM_MAC
#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#if defined(OVR_GL)
#include "OpenGLDrvPrivate.h"
#include "OpenGLResources.h"

#define OVR_SKIP_GL_SYSTEM_INC
#include "OVR_CAPI_GL.h"

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"
#include "ScreenRendering.h"

#include "SlateBasics.h"

class FOpenGLTexture2DSet : public FOpenGLTexture2D
{
public:
	FOpenGLTexture2DSet(
		class FOpenGLDynamicRHI* InGLRHI,
		GLuint InResource,
		GLenum InTarget,
		GLenum InAttachment,
		uint32 InSizeX,
		uint32 InSizeY,
		uint32 InSizeZ,
		uint32 InNumMips,
		uint32 InNumSamples,
		uint32 InArraySize,
		EPixelFormat InFormat,
		bool bInCubemap,
		bool bInAllocatedStorage,
		uint32 InFlags,
		uint8* InTextureRange
		)
		: FOpenGLTexture2D(
		InGLRHI,
		InResource,
		InTarget,
		InAttachment,
		InSizeX,
		InSizeY,
		InSizeZ,
		InNumMips,
		InNumSamples,
		InArraySize,
		InFormat,
		bInCubemap,
		bInAllocatedStorage,
		InFlags,
		InTextureRange,
		FClearValueBinding::None
		)
	{
		TextureSet = nullptr;
	}
	~FOpenGLTexture2DSet()
	{
		// Resources must be released by prior call to ReleaseResources
		check(Resource == 0);
	}

	void ReleaseResources(ovrSession InOvrSession);
	void SwitchToNextElement();
	void AddTexture(GLuint InTexture);

	ovrSwapTextureSet* GetSwapTextureSet() const { return TextureSet; }

	static FOpenGLTexture2DSet* CreateTexture2DSet(
		FOpenGLDynamicRHI* InGLRHI,
		ovrSwapTextureSet* InTextureSet,
		uint32 InNumSamples,
		EPixelFormat InFormat,
		uint32 InFlags);
protected:
	void InitWithCurrentElement();

	struct TextureElement
	{
		GLuint Texture;
	};
	TArray<TextureElement> Textures;

	ovrSwapTextureSet* TextureSet;
};

class FOpenGLTexture2DSetProxy : public FTexture2DSetProxy
{
public:
	FOpenGLTexture2DSetProxy(ovrSession InOvrSession, FTextureRHIRef InTexture, uint32 SrcSizeX, uint32 SrcSizeY, EPixelFormat SrcFormat) 
		: FTexture2DSetProxy(InOvrSession, InTexture, SrcSizeX, SrcSizeY, SrcFormat) {}

	virtual ovrSwapTextureSet* GetSwapTextureSet() const override
	{
		if (!RHITexture.IsValid())
		{
			return nullptr;
		}
		auto GLTS = static_cast<FOpenGLTexture2DSet*>(RHITexture->GetTexture2D());
		return GLTS->GetSwapTextureSet();
	}

	virtual void SwitchToNextElement() override
	{
		if (RHITexture.IsValid())
		{
			auto GLTS = static_cast<FOpenGLTexture2DSet*>(RHITexture->GetTexture2D());
			GLTS->SwitchToNextElement();
		}
	}

	virtual void ReleaseResources() override
	{
		if (RHITexture.IsValid())
		{
			auto GLTS = static_cast<FOpenGLTexture2DSet*>(RHITexture->GetTexture2D());
			GLTS->ReleaseResources(OvrSession);
			RHITexture = nullptr;
		}
	}

protected:
};

void FOpenGLTexture2DSet::AddTexture(GLuint InTexture)
{
	TextureElement element;
	element.Texture = InTexture;
	Textures.Push(element);
}

void FOpenGLTexture2DSet::SwitchToNextElement()
{
	check(TextureSet);
	check(TextureSet->TextureCount == Textures.Num());

	TextureSet->CurrentIndex = (TextureSet->CurrentIndex + 1) % TextureSet->TextureCount;
	InitWithCurrentElement();
}

void FOpenGLTexture2DSet::InitWithCurrentElement()
{
	check(TextureSet);
	check(TextureSet->TextureCount == Textures.Num());

	Resource = Textures[TextureSet->CurrentIndex].Texture;
}

void FOpenGLTexture2DSet::ReleaseResources(ovrSession InOvrSession)
{
	if (TextureSet)
	{
		ovrGLTexture GLTex0, GLTex1;
		GLTex0.Texture = TextureSet->Textures[0];
		GLTex1.Texture = TextureSet->Textures[1];
		UE_LOG(LogHMD, Log, TEXT("Releasing GL textureSet 0x%p, tex id %d, %d"), TextureSet, GLTex0.OGL.TexId, GLTex1.OGL.TexId);
		check(Textures.Num() == TextureSet->TextureCount);
		check(Textures[0].Texture == GLTex0.OGL.TexId && Textures[1].Texture == GLTex1.OGL.TexId);

		ovr_DestroySwapTextureSet(InOvrSession, TextureSet);
		TextureSet = nullptr;
	}
	Textures.Empty(0);
	Resource = 0;
}

FOpenGLTexture2DSet* FOpenGLTexture2DSet::CreateTexture2DSet(
	FOpenGLDynamicRHI* InGLRHI,
	ovrSwapTextureSet* InTextureSet,
	uint32 InNumSamples,
	EPixelFormat InFormat,
	uint32 InFlags
	)
{
	check(InTextureSet);

	GLenum Target = (InNumSamples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
	GLenum Attachment = GL_NONE;// GL_COLOR_ATTACHMENT0;
	bool bAllocatedStorage = false;
	uint32 NumMips = 1;
	uint8* TextureRange = nullptr;

	const uint32 SizeX = InTextureSet->Textures[0].Header.TextureSize.w;
	const uint32 SizeY = InTextureSet->Textures[0].Header.TextureSize.h;
	FOpenGLTexture2DSet* NewTextureSet = new FOpenGLTexture2DSet(
		InGLRHI, 0, Target, Attachment, SizeX, SizeY, 0, NumMips, InNumSamples, 1, InFormat, false, bAllocatedStorage, InFlags, TextureRange);
	OpenGLTextureAllocated(NewTextureSet, InFlags);

	UE_LOG(LogHMD, Log, TEXT("Allocated textureSet 0x%p (%d x %d)"), NewTextureSet, SizeX, SizeY);

	const uint32 TexCount = InTextureSet->TextureCount;

	for (uint32 i = 0; i < TexCount; ++i)
	{
		ovrGLTexture GLTex;
		GLTex.Texture = InTextureSet->Textures[i];

		NewTextureSet->AddTexture(GLTex.OGL.TexId);
		
		UE_LOG(LogHMD, Log, TEXT("Allocated tex %d (%d x %d), texId = %d"), i, SizeX, SizeY, GLTex.OGL.TexId);

#if 0 // This code checks if newly allocated textures a legit. Enable when needed.
		glBindTexture(Target, GLTex.OGL.TexId);
		GLint err = glGetError();
		if (err)
		{
			UE_LOG(LogHMD, Warning, TEXT("Can't bind a new tex %d, error %d"), GLTex.OGL.TexId, err);
		}

		GLuint buffer;
		glGenFramebuffers(1, &buffer);
		glBindFramebuffer(GL_FRAMEBUFFER, buffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, Target, GLTex.OGL.TexId, 0);
		GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (e != GL_FRAMEBUFFER_COMPLETE)
		{
			UE_LOG(LogHMD, Warning, TEXT("Can't bind a new tex %d to a frame buffer, error %d"), GLTex.OGL.TexId, int(e));
		}

		glDeleteFramebuffers(1, &buffer);
#endif
	}

	NewTextureSet->TextureSet = InTextureSet;
	NewTextureSet->InitWithCurrentElement();
	return NewTextureSet;
}

static FOpenGLTexture2D* OpenGLCreateTexture2DAlias(
	FOpenGLDynamicRHI* InGLRHI,
	GLuint InResource,
	uint32 InSizeX,
	uint32 InSizeY,
	uint32 InSizeZ,
	uint32 InNumMips,
	uint32 InNumSamples,
	EPixelFormat InFormat,
	uint32 InFlags)
{
	GLenum Target = (InNumSamples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
	GLenum Attachment = GL_NONE;
	bool bAllocatedStorage = false;
	uint32 NumMips = 1;
	uint8* TextureRange = nullptr;

	FOpenGLTexture2D* NewTexture = new FOpenGLTexture2D(
		InGLRHI,
		InResource,
		Target,
		Attachment, InSizeX, InSizeY, 0, InNumMips, InNumSamples, 1, InFormat, false, bAllocatedStorage, InFlags, TextureRange, FClearValueBinding::None);

	OpenGLTextureAllocated(NewTexture, InFlags);
	return NewTexture;
}

//////////////////////////////////////////////////////////////////////////
FOculusRiftHMD::OGLBridge::OGLBridge(ovrSession InOvrSession) :
	FCustomPresent()
{
	Init(InOvrSession);
}

void FOculusRiftHMD::OGLBridge::SetHmd(ovrSession InOvrSession)
{
	if (InOvrSession != OvrSession)
	{
		Reset();
		Init(InOvrSession);
		bNeedReAllocateTextureSet = true;
		bNeedReAllocateMirrorTexture = true;
	}
}

void FOculusRiftHMD::OGLBridge::Init(ovrSession InOvrSession)
{
	OvrSession = InOvrSession;
	bInitialized = true;
}

FTexture2DSetProxyRef FOculusRiftHMD::OGLBridge::CreateTextureSet(const uint32 InSizeX, const uint32 InSizeY, const EPixelFormat InFormat, uint32 InNumMips, uint32 InFlags)
{
	check(InSizeX != 0 && InSizeY != 0);

	auto GLRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);
	FResourceBulkDataInterface* BulkData = nullptr;

	const bool bSRGB = true; //(InFlags & TexCreate_SRGB) != 0;
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[InFormat];
	if (GLFormat.InternalFormat[bSRGB] == GL_NONE)
	{
		UE_LOG(LogRHI, Fatal, TEXT("Texture format '%s' not supported (sRGB=%d)."), GPixelFormats[InFormat].Name, bSRGB);
	}

	ovrSwapTextureSet* textureSet;
	ovrResult res = ovr_CreateSwapTextureSetGL(OvrSession, GLFormat.InternalFormat[bSRGB], InSizeX, InSizeY, &textureSet);
	if (!textureSet || !OVR_SUCCESS(res))
	{
		UE_LOG(LogHMD, Error, TEXT("Can't create swap texture set (size %d x %d), error = %d"), InSizeX, InSizeY, res);
		if (res == ovrError_DisplayLost)
		{
			bNeedReAllocateMirrorTexture = bNeedReAllocateTextureSet = true;
			FPlatformAtomics::InterlockedExchange(&NeedToKillHmd, 1);
		}
		return false;
	}
	UE_LOG(LogHMD, Log, TEXT("Allocated a new swap texture set (size %d x %d)"), InSizeX, InSizeY);

	TRefCountPtr<FOpenGLTexture2DSet> ColorTextureSet = FOpenGLTexture2DSet::CreateTexture2DSet(
		GLRHI,
		textureSet,
		1,
		EPixelFormat(InFormat),
		TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_SRGB
		);
	if (ColorTextureSet)
	{
		return MakeShareable(new FOpenGLTexture2DSetProxy(OvrSession, ColorTextureSet->GetTexture2D(), InSizeX, InSizeY, InFormat));
	}
	return nullptr;
}

void FOculusRiftHMD::OGLBridge::BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT)
{
	SCOPE_CYCLE_COUNTER(STAT_BeginRendering);

	SetRenderContext(&InRenderContext);

	FGameFrame* CurrentFrame = GetRenderFrame();
	check(CurrentFrame);
	FSettings* FrameSettings = CurrentFrame->GetSettings();
	check(FrameSettings);

	const uint32 RTSizeX = RT->GetSizeX();
	const uint32 RTSizeY = RT->GetSizeY();

	const FVector2D ActualMirrorWindowSize = CurrentFrame->WindowSize;
	// detect if mirror texture needs to be re-allocated or freed
	if (OvrSession && MirrorTextureRHI && (bNeedReAllocateMirrorTexture || OvrSession != RenderContext->OvrSession ||
		(FrameSettings->Flags.bMirrorToWindow && (
		FrameSettings->MirrorWindowMode != FSettings::eMirrorWindow_Distorted ||
		ActualMirrorWindowSize != FVector2D(MirrorTextureRHI->GetSizeX(), MirrorTextureRHI->GetSizeY()))) ||
		!FrameSettings->Flags.bMirrorToWindow ))
	{
		check(MirrorTexture);
		ovr_DestroyMirrorTexture(OvrSession, MirrorTexture);
		MirrorTexture = nullptr;
		MirrorTextureRHI = nullptr;
		bNeedReAllocateMirrorTexture = false;
	}

	// need to allocate a mirror texture?
	if (FrameSettings->Flags.bMirrorToWindow && FrameSettings->MirrorWindowMode == FSettings::eMirrorWindow_Distorted && !MirrorTextureRHI &&
		ActualMirrorWindowSize.X != 0 && ActualMirrorWindowSize.Y != 0)
	{
		ovrResult res = ovr_CreateMirrorTextureGL(OvrSession, GL_RGBA, ActualMirrorWindowSize.X, ActualMirrorWindowSize.Y, &MirrorTexture);
		if (!MirrorTexture || !OVR_SUCCESS(res))
		{
			UE_LOG(LogHMD, Error, TEXT("Can't create a mirror texture, error = %d"), res);
			return;
		}
		UE_LOG(LogHMD, Log, TEXT("Allocated a new mirror texture (size %d x %d)"), ActualMirrorWindowSize.X, ActualMirrorWindowSize.Y);
		ovrGLTexture GLMirrorTexture;
		GLMirrorTexture.Texture = *MirrorTexture;
		MirrorTextureRHI = OpenGLCreateTexture2DAlias(
			static_cast<FOpenGLDynamicRHI*>(GDynamicRHI),
			GLMirrorTexture.OGL.TexId,
			ActualMirrorWindowSize.X,
			ActualMirrorWindowSize.Y,
			0,
			1,
			1,
			(EPixelFormat)PF_B8G8R8A8,
			TexCreate_RenderTargetable);
		bNeedReAllocateMirrorTexture = false;
	}
}

void FOculusRiftHMD::OGLBridge::Reset_RenderThread()
{
	if (MirrorTexture)
	{
		ovr_DestroyMirrorTexture(OvrSession, MirrorTexture);
		MirrorTextureRHI = nullptr;
		MirrorTexture = nullptr;
	}
	LayerMgr.ReleaseRenderLayers_RenderThread();
	OvrSession = nullptr;

	if (RenderContext.IsValid())
	{
		RenderContext->bFrameBegun = false;
		SetRenderContext(nullptr);
	}
}


void FOculusRiftHMD::OGLBridge::Reset()
{
	if (IsInGameThread())
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(ResetOGL,
			FOculusRiftHMD::OGLBridge*, Bridge, this,
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
}

#endif // #if defined(OVR_GL)

#if PLATFORM_WINDOWS
	// It is required to undef WINDOWS_PLATFORM_TYPES_GUARD for any further D3D11 / GL private includes
	#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
#endif //#if !PLATFORM_MAC
	