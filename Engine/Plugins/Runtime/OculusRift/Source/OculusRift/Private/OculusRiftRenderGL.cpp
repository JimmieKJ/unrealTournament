// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "OculusRiftPrivatePCH.h"
#include "OculusRiftHMD.h"

#if !PLATFORM_MAC
#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#ifdef OVR_GL
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
	void SwitchToNextElement(ovrSession InOvrSession);
	void AddTexture(GLuint InTexture);

	ovrTextureSwapChain GetSwapTextureSet() const { return TextureSet; }

	static FOpenGLTexture2DSet* CreateTexture2DSet(
		FOpenGLDynamicRHI* InGLRHI,
		const FOvrSessionSharedPtr& OvrSession,
		ovrTextureSwapChain InTextureSet,
		const ovrTextureSwapChainDesc& InDesc,
		uint32 InNumSamples,
		EPixelFormat InFormat,
		uint32 InFlags);
protected:
	void InitWithCurrentElement(int CurrentIndex);

	struct TextureElement
	{
		GLuint Texture;
	};
	TArray<TextureElement> Textures;

	ovrTextureSwapChain TextureSet;
};

class FOpenGLTexture2DSetProxy : public FTexture2DSetProxy
{
public:
	FOpenGLTexture2DSetProxy(const FOvrSessionSharedPtr& InOvrSession, FTextureRHIRef InTexture, uint32 SrcSizeX, uint32 SrcSizeY, EPixelFormat SrcFormat, uint32 SrcNumMips) 
		: FTexture2DSetProxy(InOvrSession, InTexture, SrcSizeX, SrcSizeY, SrcFormat, SrcNumMips) {}

	virtual ovrTextureSwapChain GetSwapTextureSet() const override
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
			FOvrSessionShared::AutoSession OvrSession(Session);
			GLTS->SwitchToNextElement(OvrSession);
		}
	}

	virtual void ReleaseResources() override
	{
		if (RHITexture.IsValid())
		{
			auto GLTS = static_cast<FOpenGLTexture2DSet*>(RHITexture->GetTexture2D());
			FOvrSessionShared::AutoSession OvrSession(Session);
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

void FOpenGLTexture2DSet::SwitchToNextElement(ovrSession InOvrSession)
{
	check(TextureSet);

	int CurrentIndex;
	ovr_GetTextureSwapChainCurrentIndex(InOvrSession, TextureSet, &CurrentIndex);

	InitWithCurrentElement(CurrentIndex);
}

void FOpenGLTexture2DSet::InitWithCurrentElement(int CurrentIndex)
{
	check(TextureSet);

	Resource = Textures[CurrentIndex].Texture;
}

void FOpenGLTexture2DSet::ReleaseResources(ovrSession InOvrSession)
{
	if (TextureSet)
	{
		UE_LOG(LogHMD, Log, TEXT("Releasing GL textureSet 0x%p````"), TextureSet);
#if !UE_BUILD_SHIPPING
		int TexCount;
		ovr_GetTextureSwapChainLength(InOvrSession, TextureSet, &TexCount);
		check(Textures.Num() == TexCount);
		
		for (int i = 0; i < TexCount; ++i)
		{
			GLuint TexId;
			ovrResult res = ovr_GetTextureSwapChainBufferGL(InOvrSession, TextureSet, i, &TexId);
			if (!OVR_SUCCESS(res))
			{
				continue;
			}
			check(TexId == Textures[i].Texture);
		}
#endif

		ovr_DestroyTextureSwapChain(InOvrSession, TextureSet);
		TextureSet = nullptr;
	}
	Textures.Empty(0);
	Resource = 0;
}

FOpenGLTexture2DSet* FOpenGLTexture2DSet::CreateTexture2DSet(
	FOpenGLDynamicRHI* InGLRHI,
	const FOvrSessionSharedPtr& Session,
	ovrTextureSwapChain InTextureSet,
	const ovrTextureSwapChainDesc& InDesc,
	uint32 InNumSamples,
	EPixelFormat InFormat,
	uint32 InFlags
	)
{
	check(InTextureSet);

	if (InFormat == PF_B8G8R8A8)
	{
		InFormat = PF_R8G8B8A8;
	}
	InFlags |= TexCreate_SRGB;

	GLenum Target = (InNumSamples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
	GLenum Attachment = GL_NONE;// GL_COLOR_ATTACHMENT0;
	bool bAllocatedStorage = false;
	uint8* TextureRange = nullptr;
	ovrResult res;

	const uint32 SizeX = uint32(InDesc.Width);
	const uint32 SizeY = uint32(InDesc.Height);
	FOpenGLTexture2DSet* NewTextureSet = new FOpenGLTexture2DSet(
		InGLRHI, 0, Target, Attachment, SizeX, SizeY, 0, InDesc.MipLevels, InNumSamples, 1, InFormat, false, bAllocatedStorage, InFlags, TextureRange);
	OpenGLTextureAllocated(NewTextureSet, InFlags);

	UE_LOG(LogHMD, Log, TEXT("Allocated textureSet 0x%p (%d x %d)"), NewTextureSet, SizeX, SizeY);

	int TexCount;
	FOvrSessionShared::AutoSession OvrSession(Session);
	ovr_GetTextureSwapChainLength(OvrSession, InTextureSet, &TexCount);

	for (int32 i = 0; i < TexCount; ++i)
	{
		GLuint GLTextureId;
		res = ovr_GetTextureSwapChainBufferGL(OvrSession, InTextureSet, i, &GLTextureId);
		if (!OVR_SUCCESS(res))
		{
			UE_LOG(LogHMD, Error, TEXT("ovr_GetTextureSwapChainBufferGL failed, error = %d"), int(res));
			return nullptr;
		}

		NewTextureSet->AddTexture(GLTextureId);
		
		UE_LOG(LogHMD, Log, TEXT("Allocated tex %d (%d x %d), texId = %d"), i, SizeX, SizeY, GLTextureId);

#if 0 // This code checks if newly allocated textures a legit. Enable when needed.
		glBindTexture(Target, GLTextureId);
		GLint err = glGetError();
		if (err)
		{
			UE_LOG(LogHMD, Warning, TEXT("Can't bind a new tex %d, error %d"), GLTextureId, err);
		}

		GLuint buffer;
		glGenFramebuffers(1, &buffer);
		glBindFramebuffer(GL_FRAMEBUFFER, buffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, Target, GLTextureId, 0);
		GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (e != GL_FRAMEBUFFER_COMPLETE)
		{
			UE_LOG(LogHMD, Warning, TEXT("Can't bind a new tex %d to a frame buffer, error %d"), GLTex.OGL.TexId, int(e));
		}

		glDeleteFramebuffers(1, &buffer);
#endif
	}

	NewTextureSet->TextureSet = InTextureSet;
	NewTextureSet->InitWithCurrentElement(0);
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
FOculusRiftHMD::OGLBridge::OGLBridge(const FOvrSessionSharedPtr& InOvrSession) :
	FCustomPresent(InOvrSession)
{
}

bool FOculusRiftHMD::OGLBridge::IsUsingGraphicsAdapter(const ovrGraphicsLuid& luid)
{
	// UNDONE
	return true;
}

FTexture2DSetProxyPtr FOculusRiftHMD::OGLBridge::CreateTextureSet(const uint32 InSizeX, const uint32 InSizeY, const EPixelFormat InSrcFormat, const uint32 InNumMips, uint32 InCreateTexFlags)
{
	check(InSizeX != 0 && InSizeY != 0);

	auto GLRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);
	FResourceBulkDataInterface* BulkData = nullptr;

	const bool bSRGB = true; //(InFlags & TexCreate_SRGB) != 0;
	const EPixelFormat Format = PF_R8G8B8A8;
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Format];
	if (GLFormat.InternalFormat[bSRGB] == GL_NONE)
	{
		UE_LOG(LogRHI, Fatal, TEXT("Texture format '%s' not supported (sRGB=%d)."), GPixelFormats[Format].Name, bSRGB);
	}

	const uint32 TexCreate_Flags = (((InCreateTexFlags & ShaderResource) ? TexCreate_ShaderResource : 0) |
									((InCreateTexFlags & RenderTargetable) ? TexCreate_RenderTargetable : 0))  | TexCreate_SRGB;

	ovrTextureSwapChainDesc desc{};
	desc.Type = ovrTexture_2D;
	desc.ArraySize = 1;
	desc.MipLevels = (InNumMips == 0) ? GetNumMipLevels(InSizeX, InSizeY, InCreateTexFlags) : InNumMips;
	check(desc.MipLevels > 0);
	desc.SampleCount = 1;
	desc.StaticImage = (InCreateTexFlags & StaticImage) ? ovrTrue : ovrFalse;
	desc.Width = InSizeX;
	desc.Height = InSizeY;
	// Override the format to be sRGB so that the compositor always treats eye buffers
	// as if they're sRGB even if we are sending in linear formatted textures
	desc.Format = (InCreateTexFlags & LinearSpace) ? OVR_FORMAT_R8G8B8A8_UNORM : OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.MiscFlags = 0;
	desc.BindFlags = 0;
	if (desc.MipLevels != 1)
	{
		desc.MiscFlags |= ovrTextureMisc_AllowGenerateMips;
	}
	ovrTextureSwapChain textureSet;
	FOvrSessionShared::AutoSession OvrSession(Session);
	ovrResult res = ovr_CreateTextureSwapChainGL(OvrSession, &desc, &textureSet);
	if (!textureSet || !OVR_SUCCESS(res))
	{
		UE_LOG(LogHMD, Error, TEXT("Can't create swap texture set (size %d x %d), error = %d"), desc.Width, desc.Height, res);
		if (res == ovrError_DisplayLost)
		{
			bNeedReAllocateMirrorTexture = bNeedReAllocateTextureSet = true;
			FPlatformAtomics::InterlockedExchange(&NeedToKillHmd, 1);
		}
		return false;
	}
	bReady = true;
	UE_LOG(LogHMD, Log, TEXT("Allocated a new swap texture set (size %d x %d, mipcount = %d), 0x%p"), desc.Width, desc.Height, desc.MipLevels, textureSet);

	TRefCountPtr<FOpenGLTexture2DSet> ColorTextureSet = FOpenGLTexture2DSet::CreateTexture2DSet(
		GLRHI,
		Session,
		textureSet,
		desc,
		1,
		Format,
		TexCreate_Flags
		);
	if (ColorTextureSet)
	{
		return MakeShareable(new FOpenGLTexture2DSetProxy(Session, ColorTextureSet->GetTexture2D(), InSizeX, InSizeY, InSrcFormat, InNumMips));
	}
	return nullptr;
}

void FOculusRiftHMD::OGLBridge::BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT)
{
	SCOPE_CYCLE_COUNTER(STAT_BeginRendering);

	SetRenderContext(&InRenderContext);

	FGameFrame* ThisFrame = GetRenderFrame();
	check(ThisFrame);
	FSettings* FrameSettings = ThisFrame->GetSettings();
	check(FrameSettings);

	const uint32 RTSizeX = RT->GetSizeX();
	const uint32 RTSizeY = RT->GetSizeY();

	const FVector2D ActualMirrorWindowSize = ThisFrame->WindowSize;
	// detect if mirror texture needs to be re-allocated or freed
	FOvrSessionShared::AutoSession OvrSession(Session);
	if (Session->IsActive() && MirrorTextureRHI && (bNeedReAllocateMirrorTexture || 
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
		ovrMirrorTextureDesc desc{};
		// Override the format to be sRGB so that the compositor always treats eye buffers
		// as if they're sRGB even if we are sending in linear format textures
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.Width = (int)ActualMirrorWindowSize.X;
		desc.Height = (int)ActualMirrorWindowSize.Y;
		desc.MiscFlags = 0;

		ovrResult res = ovr_CreateMirrorTextureGL(OvrSession, &desc, &MirrorTexture);
		if (!MirrorTexture || !OVR_SUCCESS(res))
		{
			UE_LOG(LogHMD, Error, TEXT("Can't create a mirror texture, error = %d"), res);
			return;
		}
		bReady = true;
		UE_LOG(LogHMD, Log, TEXT("Allocated a new mirror texture (size %d x %d)"), desc.Width, desc.Height);
		
		GLuint GLMirrorTexture;
		res = ovr_GetMirrorTextureBufferGL(OvrSession, MirrorTexture, &GLMirrorTexture);
		if (!OVR_SUCCESS(res))
		{
			UE_LOG(LogHMD, Error, TEXT("ovr_GetMirrorTextureBufferGL failed, error = %d"), int(res));
			return;
		}

		MirrorTextureRHI = OpenGLCreateTexture2DAlias(
			static_cast<FOpenGLDynamicRHI*>(GDynamicRHI),
			GLMirrorTexture,
			desc.Width,
			desc.Height,
			0,
			1,
			1,
			(EPixelFormat)PF_R8G8B8A8,
			TexCreate_RenderTargetable);
		bNeedReAllocateMirrorTexture = false;
	}
}

#endif // #ifdef OVR_GL

#if PLATFORM_WINDOWS
	// It is required to undef WINDOWS_PLATFORM_TYPES_GUARD for any further D3D11 / GL private includes
	#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
#endif //#if !PLATFORM_MAC
	