// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Misc/MediaTextureResource.h"
#include "RenderingThread.h"
#include "Shader.h"
#include "IMediaTextureSink.h"
#include "MediaTexture.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "RHIStaticStates.h"
#include "MediaShaders.h"
#include "MediaAssetsPrivate.h"


/* FMediaTextureResource structors
 *****************************************************************************/

FMediaTextureResource::FMediaTextureResource(UMediaTexture& InOwner, const FLinearColor& InClearColor, const FIntPoint& InOutputDimensions, EMediaTextureSinkFormat InSinkFormat, EMediaTextureSinkMode InSinkMode)
	: Owner(InOwner)
	, TripleBuffer(BufferResources)
	, BufferClearColor(InClearColor)
	, BufferDimensions(InOutputDimensions)
	, BufferPitch(0)
	, CachedResourceSizeBytes(0)
	, OutputDimensions(InOutputDimensions)
	, RequiresConversion(false)
	, SinkFormat(InSinkFormat)
	, SinkMode(InSinkMode)
	, State(EState::ShutDown)
{ }


FMediaTextureResource::~FMediaTextureResource()
{
	FlushRenderingCommands();
}


/* FMediaTextureResource interface
 *****************************************************************************/

void* FMediaTextureResource::AcquireBuffer()
{
	if (State != EState::Initialized)
	{
		return nullptr;
	}

	if (SinkMode == EMediaTextureSinkMode::Buffered)
	{
		return TripleBuffer.GetWriteBuffer().LockedData;
	}

	check(IsInActualRenderingThread());

	uint32 OutStride = 0;
	TRefCountPtr<FRHITexture2D>& Texture = RequiresConversion
		? BufferResources[2].RenderTarget
		: RenderTargetTextureRHI;

	return RHILockTexture2D(Texture.GetReference(), 0, RLM_WriteOnly, OutStride, false /*bLockWithinMiptail*/, false /*bFlushRHIThread*/);
}


void FMediaTextureResource::DisplayBuffer()
{
	if (State != EState::Initialized)
	{
		return;
	}

	if (IsInActualRenderingThread())
	{
		SwapResource();
	}
	else
	{
		RenderThreadTasks.Enqueue([this]() {
			SwapResource();
		});
	}
}


FRHITexture* FMediaTextureResource::GetTexture()
{
	if (State != EState::Initialized)
	{
		return nullptr;
	}

	check(IsInRenderingThread());
	check(SinkMode == EMediaTextureSinkMode::Unbuffered);

	return RenderTargetTextureRHI;
}


void FMediaTextureResource::InitializeBuffer(FIntPoint OutputDim, FIntPoint BufferDim, EMediaTextureSinkFormat Format, EMediaTextureSinkMode Mode)
{
	State = EState::Initializing;

	if (IsInActualRenderingThread())
	{
		InitializeResource(OutputDim, BufferDim, Format, Mode);
	}
	else
	{
		RenderThreadTasks.Enqueue([=]() {
			InitializeResource(OutputDim, BufferDim, Format, Mode);
		});
	}
}


void FMediaTextureResource::ReleaseBuffer()
{
	if (State != EState::Initialized)
	{
		return;
	}

	if (SinkMode == EMediaTextureSinkMode::Buffered)
	{
		TripleBuffer.SwapWriteBuffers();
	}
	else
	{
		check(IsInActualRenderingThread());

		TRefCountPtr<FRHITexture2D>& Texture = RequiresConversion
			? BufferResources[2].RenderTarget
			: RenderTargetTextureRHI;

		RHIUnlockTexture2D(Texture.GetReference(), 0, false /*bLockWithinMiptail*/);
	}
}


void FMediaTextureResource::ShutdownBuffer()
{
	if ((State == EState::ShutDown) || (State == EState::ShuttingDown))
	{
		return;
	}

	State = EState::ShuttingDown;

	if (IsInActualRenderingThread())
	{
		ReleaseDynamicRHI();
	}
	else
	{
		RenderThreadTasks.Enqueue([this]() {
			ReleaseDynamicRHI();
		});
	}
}


void FMediaTextureResource::UpdateBuffer(const uint8* Data, uint32 /*Pitch*/)
{
	if (State != EState::Initialized)
	{
		return;
	}

	// update sink
	if (SinkMode == EMediaTextureSinkMode::Buffered)
	{
		uint8* Dest = (uint8*)TripleBuffer.GetWriteBuffer().LockedData;

		if (Dest != nullptr)
		{
			FMemory::Memcpy(Dest, Data, BufferPitch * BufferDimensions.Y);

			if (TripleBuffer.IsDirty())
			{
				UE_LOG(LogMediaAssets, VeryVerbose, TEXT("MediaTextureResource frame dropped."));
			}

			TripleBuffer.SwapWriteBuffers();
		}
	}
	else if (IsInActualRenderingThread())
	{
		if (RequiresConversion)
		{
			FUpdateTextureRegion2D Region(0, 0, 0, 0, BufferDimensions.X, BufferDimensions.Y);
			RHIUpdateTexture2D(BufferResources[2].RenderTarget.GetReference(), 0, Region, BufferPitch, Data);
			ConvertResource(BufferResources[2]);
		}
		else
		{
			FUpdateTextureRegion2D Region(0, 0, 0, 0, OutputDimensions.X, OutputDimensions.Y);
			RHIUpdateTexture2D(RenderTargetTextureRHI.GetReference(), 0, Region, BufferPitch, Data);
		}
	}
	else
	{
		UE_LOG(LogMediaAssets, Error, TEXT("UpdateTextureSinkBuffer must be called on render thread in Unbuffered mode."));
	}
}


void FMediaTextureResource::UpdateTextures(FRHITexture* RenderTarget, FRHITexture* ShaderResource)
{
	if (State != EState::Initialized)
	{
		return;
	}

	check(SinkMode == EMediaTextureSinkMode::Unbuffered);

	FResource Resource;
	{
		Resource.RenderTarget = (RenderTarget != nullptr) ? RenderTarget->GetTexture2D() : nullptr;
		Resource.ShaderResource = (ShaderResource != nullptr) ? ShaderResource->GetTexture2D() : nullptr;
	}

	if (IsInActualRenderingThread())
	{
		SetResource(Resource);
	}
	else
	{
		RenderThreadTasks.Enqueue([=]() {
			SetResource(Resource);
		});
	}
}


/* FRenderTarget interface
 *****************************************************************************/

FIntPoint FMediaTextureResource::GetSizeXY() const
{
	return OutputDimensions;
}


/* FTextureResource interface
 *****************************************************************************/

FString FMediaTextureResource::GetFriendlyName() const
{
	return Owner.GetPathName();
}


uint32 FMediaTextureResource::GetSizeX() const
{
	return OutputDimensions.X;
}


uint32 FMediaTextureResource::GetSizeY() const
{
	return OutputDimensions.Y;
}


void FMediaTextureResource::InitDynamicRHI()
{
	// create the sampler state
	FSamplerStateInitializerRHI SamplerStateInitializer(
		(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(&Owner),
		(Owner.AddressX == TA_Wrap) ? AM_Wrap : ((Owner.AddressX == TA_Clamp) ? AM_Clamp : AM_Mirror),
		(Owner.AddressY == TA_Wrap) ? AM_Wrap : ((Owner.AddressY == TA_Clamp) ? AM_Clamp : AM_Mirror),
		AM_Wrap
	);

	SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);

	if ((OutputDimensions.GetMin() <= 0) || (BufferDimensions.GetMin() <= 0))
	{
		return;
	}

	// determine buffer resource pixel format
	EPixelFormat BufferFormat;
	bool SrgbBuffer = false;

	switch (SinkFormat)
	{
	case EMediaTextureSinkFormat::CharNV12:
	case EMediaTextureSinkFormat::CharNV21:
		BufferFormat = PF_G8;
		BufferPitch = BufferDimensions.X;
		RequiresConversion = true;
		break;

	case EMediaTextureSinkFormat::CharUYVY:
	case EMediaTextureSinkFormat::CharYUY2:
	case EMediaTextureSinkFormat::CharYVYU:
		BufferFormat = PF_B8G8R8A8;
		BufferPitch = BufferDimensions.X * 4;
		RequiresConversion = true;
		break;

	case EMediaTextureSinkFormat::CharAYUV:
		BufferFormat = PF_B8G8R8A8;
		BufferPitch = BufferDimensions.X * 4;
		RequiresConversion = true;
		break;

	case EMediaTextureSinkFormat::CharBGRA:
		BufferFormat = PF_B8G8R8A8;
		BufferPitch = BufferDimensions.X * 4;
		RequiresConversion = (BufferDimensions != OutputDimensions);
		SrgbBuffer = true;
		break;

	case EMediaTextureSinkFormat::CharBMP:
		BufferFormat = PF_B8G8R8A8;
		BufferPitch = BufferDimensions.X * 4;
		RequiresConversion = true;
		SrgbBuffer = true;
		break;

	case EMediaTextureSinkFormat::FloatRGB:
		BufferFormat = PF_FloatRGB;
		BufferPitch = BufferDimensions.X * 12;
		RequiresConversion = (BufferDimensions != OutputDimensions);
		SrgbBuffer = true;
		break;

	case EMediaTextureSinkFormat::FloatRGBA:
		BufferFormat = PF_FloatRGBA;
		BufferPitch = BufferDimensions.X * 16;
		RequiresConversion = (BufferDimensions != OutputDimensions);
		SrgbBuffer = true;
		break;

	default:
		return;
	}

	// determine how many buffer resources we need
	int32 StartIndex;
	
	if (SinkMode == EMediaTextureSinkMode::Buffered)
	{
		StartIndex = 0; // triple buffer
	}
	else if (RequiresConversion)
	{
		StartIndex = 2; // single buffer (frame before pixel format conversion)
	}
	else
	{
		StartIndex = 3; // no buffering (directly writing to output or replacing resources)
	}

	// create buffer resources
	if (StartIndex < 3)
	{
		TripleBuffer.Reset();

		FRHIResourceCreateInfo CreateInfo;
		uint32 OutStride = 0;

		// only one mip is used & textures are targetable or resolve
		for (int32 ResourceIndex = StartIndex; ResourceIndex < 3; ++ResourceIndex)
		{
			FResource& Resource = BufferResources[ResourceIndex];

			RHICreateTargetableShaderResource2D(
				BufferDimensions.X,
				BufferDimensions.Y,
				BufferFormat,
				1,
				TexCreate_Dynamic | (SrgbBuffer ? TexCreate_SRGB : 0),
				TexCreate_RenderTargetable,
				false,
				CreateInfo,
				Resource.RenderTarget,
				Resource.ShaderResource
			);

			if (ResourceIndex < 2)
			{
				// lock write & temp buffers (in triple buffered mode only)
				Resource.LockedData = RHILockTexture2D(Resource.RenderTarget.GetReference(), 0, RLM_WriteOnly, OutStride, false);
			}
			else
			{
				Resource.LockedData = nullptr;
			}
		}
	}

	// create output resource if needed
	if (RequiresConversion || (SinkMode == EMediaTextureSinkMode::Unbuffered))
	{
		// In Unbuffered mode or in Buffered mode (with conversion), this
		// resource is used as a render target for the output frame.

		FRHIResourceCreateInfo CreateInfo;
		TRefCountPtr<FRHITexture2D> Texture2DRHI;

		RHICreateTargetableShaderResource2D(
			OutputDimensions.X,
			OutputDimensions.Y,
			RequiresConversion ? PF_B8G8R8A8 : BufferFormat,
			1,
			TexCreate_Dynamic | (Owner.SRGB ? TexCreate_SRGB : 0),
			TexCreate_RenderTargetable,
			false,
			CreateInfo,
			RenderTargetTextureRHI,
			Texture2DRHI
		);

		TextureRHI = (FTextureRHIRef&)Texture2DRHI;
	}
	else
	{
		check(StartIndex < 3);

		// in Buffered mode, we pick the current read buffer as the first resource
		// to be displayed. In Unbuffered mode (with conversion), the read buffer
		// is used as the output render target.

		const FResource& Resource = TripleBuffer.Read();
		
		RenderTargetTextureRHI = Resource.RenderTarget;
		TextureRHI = Resource.ShaderResource;
	}

	// set the initial resource to be displayed
	RHIUpdateTextureReference(Owner.TextureReference.TextureReferenceRHI, RenderTargetTextureRHI.GetReference());

	// clear the displayed resource
	FRHICommandListImmediate& CommandList = FRHICommandListExecutor::GetImmediateCommandList();
	{
		SetRenderTarget(CommandList, RenderTargetTextureRHI, FTextureRHIRef());
		CommandList.SetViewport(0, 0, 0.0f, OutputDimensions.X, OutputDimensions.Y, 1.0f);
		CommandList.ClearColorTexture(RenderTargetTextureRHI, BufferClearColor, FIntRect());
		CommandList.CopyToResolveTarget(TextureRHI, TextureRHI, true, FResolveParams());
	}

	CacheResourceSize();

	State = EState::Initialized;
	FPlatformMisc::MemoryBarrier();
}


void FMediaTextureResource::ReleaseDynamicRHI()
{
	// release output resource if needed
	if (RequiresConversion || (SinkMode == EMediaTextureSinkMode::Unbuffered))
	{
		RenderTargetTextureRHI.SafeRelease();
		TextureRHI.SafeRelease();
	}

	// release buffer resources
	int32 StartIndex;

	if (SinkMode == EMediaTextureSinkMode::Buffered)
	{
		StartIndex = 0; // triple buffer
	}
	else if (RequiresConversion)
	{
		StartIndex = 2; // single buffer (for pixel format conversion)
	}
	else
	{
		StartIndex = 3; // no buffering (writing directly to output)
	}

	for (int32 ResourceIndex = StartIndex; ResourceIndex < 3; ++ResourceIndex)
	{
		FResource& Resource = BufferResources[ResourceIndex];

		if (Resource.LockedData != nullptr)
		{
			RHIUnlockTexture2D(Resource.RenderTarget.GetReference(), 0, false);
			Resource.LockedData = nullptr;
		}

		Resource.ShaderResource.SafeRelease();
		Resource.RenderTarget.SafeRelease();
	}

	// clean up
	if (SinkMode == EMediaTextureSinkMode::Buffered)
	{
		TripleBuffer.Reset();
	}

	CacheResourceSize();

	State = EState::ShutDown;
	FPlatformMisc::MemoryBarrier();
}


/* FTickerObjectBase interface
 *****************************************************************************/

bool FMediaTextureResource::Tick(float DeltaTime)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(TickMediaTextureResource, FMediaTextureResource*, This, this,
	{
		This->ProcessRenderThreadTasks();
	});

	return true;
}


/* FMediaTextureResource implementation
 *****************************************************************************/

void FMediaTextureResource::CacheResourceSize()
{
	CachedResourceSizeBytes = 0;

	if (OutputDimensions.GetMin() > 0)
	{
		const SIZE_T BufferSize = BufferPitch * BufferDimensions.Y;

		if (SinkMode == EMediaTextureSinkMode::Buffered)
		{
			CachedResourceSizeBytes += (3 * BufferSize);
		}
		else
		{
			CachedResourceSizeBytes += BufferSize;
		}

		if (RequiresConversion)
		{
			CachedResourceSizeBytes += (OutputDimensions.X * OutputDimensions.Y * 4);
		}
	}
}


void FMediaTextureResource::ConvertResource(const FResource& Resource)
{
	check(IsInRenderingThread());

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	SCOPED_DRAW_EVENT(RHICmdList, MediaTextureConvertResource);

	// configure media shaders
	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FMediaShadersVS> VertexShader(ShaderMap);

	switch (SinkFormat)
	{
	case EMediaTextureSinkFormat::CharAYUV:
		{
			TShaderMapRef<FAYUVConvertPS> ConvertShader(ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;

			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource);
		}
		break;

	case EMediaTextureSinkFormat::CharNV12:
		{
			TShaderMapRef<FNV12ConvertPS> ConvertShader(ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;

			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource, OutputDimensions);
		}
		break;

	case EMediaTextureSinkFormat::CharNV21:
		{
			TShaderMapRef<FNV21ConvertPS> ConvertShader(ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;

			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource, OutputDimensions);
		}
		break;

	case EMediaTextureSinkFormat::CharUYVY:
		{
			TShaderMapRef<FUYVYConvertPS> ConvertShader(ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;

			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource);
		}
		break;

	case EMediaTextureSinkFormat::CharYUY2:
		{
			TShaderMapRef<FYUY2ConvertPS> ConvertShader(ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;

			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource, OutputDimensions);
		}
		break;

	case EMediaTextureSinkFormat::CharYVYU:
		{
			TShaderMapRef<FYVYUConvertPS> ConvertShader(ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;

			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource);
		}
		break;

	case EMediaTextureSinkFormat::CharBGRA:
	case EMediaTextureSinkFormat::FloatRGB:
	case EMediaTextureSinkFormat::FloatRGBA:
		{
			TShaderMapRef<FRGBConvertPS> ConvertShader(ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;

			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource, OutputDimensions);
		}
		break;

	case EMediaTextureSinkFormat::CharBMP:
		{
			TShaderMapRef<FBMPConvertPS> ConvertShader(ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;

			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource, OutputDimensions);
		}
		break;

	default:
		return;
	}

	// draw full size quad into render target
	FMediaElementVertex Vertices[4];
	{
		Vertices[0].Position.Set(-1.0f, 1.0f, 1.0f, 1.0f);
		Vertices[0].TextureCoordinate.Set(0.0f, 0.0f);
		Vertices[1].Position.Set(1.0f, 1.0f, 1.0f, 1.0f);
		Vertices[1].TextureCoordinate.Set(1.0f, 0.0f);
		Vertices[2].Position.Set(-1.0f, -1.0f, 1.0f, 1.0f);
		Vertices[2].TextureCoordinate.Set(0.0f, 1.0f);
		Vertices[3].Position.Set(1.0f, -1.0f, 1.0f, 1.0f);
		Vertices[3].TextureCoordinate.Set(1.0f, 1.0f);
	}

	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHICmdList.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI());
	RHICmdList.SetViewport(0, 0, 0.0f, OutputDimensions.X, OutputDimensions.Y, 1.0f);

	FRHITexture* RenderTarget = RenderTargetTextureRHI.GetReference();
	SetRenderTargets(RHICmdList, 1, &RenderTarget, nullptr, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthNop_StencilNop);
	DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));

	RHICmdList.CopyToResolveTarget(RenderTargetTextureRHI, RenderTargetTextureRHI, true, FResolveParams());
}


void FMediaTextureResource::DisplayResource(const FResource& Resource)
{
	check(IsInRenderingThread());

	RenderTargetTextureRHI = Resource.RenderTarget;
	TextureRHI = Resource.ShaderResource;

	RHIUpdateTextureReference(Owner.TextureReference.TextureReferenceRHI, RenderTargetTextureRHI.GetReference());
}


void FMediaTextureResource::InitializeResource(FIntPoint OutputDim, FIntPoint BufferDim, EMediaTextureSinkFormat Format, EMediaTextureSinkMode Mode)
{
	check(IsInRenderingThread());

	if ((OutputDim == OutputDimensions) && (BufferDim == BufferDimensions) && (Format == SinkFormat) && (Mode == SinkMode))
	{
		// reuse existing resources
		State = (OutputDimensions.GetMin() > 0) ? EState::Initialized : EState::ShutDown;
		FPlatformMisc::MemoryBarrier();
	}
	else
	{
		// reinitialize resources
		ReleaseDynamicRHI();
		BufferDimensions = BufferDim;
		OutputDimensions = OutputDim;
		SinkFormat = Format;
		SinkMode = Mode;
		InitDynamicRHI();
	}
}


void FMediaTextureResource::ProcessRenderThreadTasks()
{
	TFunction<void()> Task;

	while (RenderThreadTasks.Dequeue(Task))
	{
		Task();
	}
}


void FMediaTextureResource::SetResource(const FResource& Resource)
{
	check(IsInRenderingThread());

	if (!Resource.RenderTarget.IsValid() || !Resource.ShaderResource.IsValid())
	{
		ReleaseDynamicRHI();
		InitDynamicRHI();
	}
	else if (RequiresConversion)
	{
		ConvertResource(Resource);
	}
	else
	{
		DisplayResource(Resource);
	}
}


void FMediaTextureResource::SwapResource()
{
	if (SinkMode == EMediaTextureSinkMode::Buffered)
	{
		if (TripleBuffer.IsDirty())
		{
			// lock & swap out old read buffer
			FResource& OldResource = TripleBuffer.Read();
			uint32 OutStride = 0;
			OldResource.LockedData = RHILockTexture2D(OldResource.RenderTarget.GetReference(), 0, RLM_WriteOnly, OutStride, false /*bLockWithinMiptail*/, false /*bFlushRHIThread*/);
			TripleBuffer.SwapReadBuffers();

			// unlock & assign new read buffer
			FResource& NewResource = TripleBuffer.Read();
			RHIUnlockTexture2D(NewResource.RenderTarget.GetReference(), 0, false /*bLockWithinMiptail*/);
			NewResource.LockedData = nullptr;

			if (RequiresConversion)
			{
				ConvertResource(NewResource);
			}
			else
			{
				DisplayResource(NewResource);
			}
		}
	}
	else if (RequiresConversion)
	{
		ConvertResource(BufferResources[2]);
	}
}
