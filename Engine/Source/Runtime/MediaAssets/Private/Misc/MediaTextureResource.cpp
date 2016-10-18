// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPCH.h"
#include "MediaTexture.h"
#include "MediaTextureResource.h"


/* FMediaTextureResource structors
 *****************************************************************************/

FMediaTextureResource::FMediaTextureResource(UMediaTexture& InOwner, const FLinearColor& InClearColor, const FIntPoint& InOutputDimensions, EMediaTextureSinkFormat InSinkFormat, EMediaTextureSinkMode InSinkMode)
	: Owner(InOwner)
	, TripleBuffer(BufferResources)
	, BufferBytesPerPixel(0)
	, BufferClearColor(InClearColor)
	, BufferDimensions(FIntPoint::ZeroValue)
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


SIZE_T FMediaTextureResource::GetResourceSize() const
{
	SIZE_T ResourceSize = 0;

	if (OutputDimensions.GetMin() > 0)
	{
		const SIZE_T BufferSize = BufferDimensions.X * BufferDimensions.Y * BufferBytesPerPixel;

		if (SinkMode == EMediaTextureSinkMode::Buffered)
		{
			ResourceSize += 3 * BufferSize;
		}
		else
		{
			ResourceSize += BufferSize;
		}

		if (RequiresConversion)
		{
			ResourceSize += OutputDimensions.X * OutputDimensions.Y * 4;
		}
	}

	return ResourceSize;
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


void FMediaTextureResource::InitializeBuffer(FIntPoint Dimensions, EMediaTextureSinkFormat Format, EMediaTextureSinkMode Mode)
{
	State = EState::Initializing;

	if (IsInActualRenderingThread())
	{
		InitializeResource(Dimensions, Format, Mode);
	}
	else
	{
		RenderThreadTasks.Enqueue([=]() {
			InitializeResource(Dimensions, Format, Mode);
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


void FMediaTextureResource::UpdateBuffer(const uint8* Data, uint32 Pitch)
{
	if (State != EState::Initialized)
	{
		return;
	}

	// calculate pitch
	const SIZE_T BytesPerRow = OutputDimensions.X * BufferBytesPerPixel;

	if (Pitch == 0)
	{
		Pitch = BytesPerRow;
	}

	// update sink
	if (SinkMode == EMediaTextureSinkMode::Buffered)
	{
		uint8* Dest = (uint8*)TripleBuffer.GetWriteBuffer().LockedData;

		if (Dest != nullptr)
		{
			if (Pitch != BytesPerRow)
			{
				const uint8* Row = Data;

				for (int32 RowIndex = 0; RowIndex < BufferDimensions.Y; ++RowIndex)
				{
					FMemory::Memcpy(Dest, Row, BytesPerRow);
					Dest += BytesPerRow;
					Row += Pitch;
				}
			}
			else
			{
				FMemory::Memcpy(Dest, Data, BufferDimensions.Y * BytesPerRow);
			}

			TripleBuffer.SwapWriteBuffers();
		}
	}
	else if (IsInActualRenderingThread())
	{
		if (RequiresConversion)
		{
			FUpdateTextureRegion2D Region(0, 0, 0, 0, BufferDimensions.X, BufferDimensions.Y);
			RHIUpdateTexture2D(BufferResources[2].RenderTarget.GetReference(), 0, Region, Pitch, Data);
			ConvertResource(BufferResources[2]);
		}
		else
		{
			FUpdateTextureRegion2D Region(0, 0, 0, 0, OutputDimensions.X, OutputDimensions.Y);
			RHIUpdateTexture2D(RenderTargetTextureRHI.GetReference(), 0, Region, Pitch, Data);
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

	if (OutputDimensions.GetMin() <= 0)
	{
		return;
	}

	// determine buffer resource pixel format
	EPixelFormat BufferFormat;
	{
		switch (SinkFormat)
		{
		case EMediaTextureSinkFormat::CharNV12:
		case EMediaTextureSinkFormat::CharNV21:
			BufferBytesPerPixel = 1;
			BufferDimensions = FIntPoint(OutputDimensions.X, OutputDimensions.Y * 1.5f);
			BufferFormat = PF_G8;
			RequiresConversion = true;
			break;

		case EMediaTextureSinkFormat::CharUYVY:
		case EMediaTextureSinkFormat::CharYUY2:
		case EMediaTextureSinkFormat::CharYVYU:
			if ((OutputDimensions.X & 0x1) != 0)
			{
				return;
			}

			BufferBytesPerPixel = 2;
			BufferDimensions = FIntPoint(OutputDimensions.X / 2, OutputDimensions.Y);
			BufferFormat = PF_B8G8R8A8;
			RequiresConversion = true;
			break;

		case EMediaTextureSinkFormat::CharAYUV:
		case EMediaTextureSinkFormat::CharBGRA:
			BufferBytesPerPixel = 4;
			BufferDimensions = OutputDimensions;
			BufferFormat = PF_B8G8R8A8;
			RequiresConversion = false;
			break;

		case EMediaTextureSinkFormat::FloatRGB:
			BufferBytesPerPixel = 12;
			BufferDimensions = OutputDimensions;
			BufferFormat = PF_FloatRGB;
			RequiresConversion = false;
			break;

		case EMediaTextureSinkFormat::FloatRGBA:
			BufferBytesPerPixel = 16;
			BufferDimensions = OutputDimensions;
			BufferFormat = PF_FloatRGBA;
			RequiresConversion = false;
			break;

		default:
			return;
		}
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
				TexCreate_Dynamic | TexCreate_NoTiling,
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
			TexCreate_Dynamic | TexCreate_NoTiling | (Owner.SRGB ? TexCreate_SRGB : 0),
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
		CommandList.Clear(true, BufferClearColor, false, 0.0f, false, 0, FIntRect());
		CommandList.CopyToResolveTarget(TextureRHI, TextureRHI, true, FResolveParams());
	}

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

	State = EState::ShutDown;
	FPlatformMisc::MemoryBarrier();
}


/* FTickerObjectBase interface
 *****************************************************************************/

bool FMediaTextureResource::Tick(float DeltaTime)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(TickMediaTextureResource, FMediaTextureResource*, This, this,
	{
		TFunction<void()> Task;

		while (This->RenderThreadTasks.Dequeue(Task))
		{
			Task();
		}
	});

	return true;
}


/* FMediaTextureResource implementation
 *****************************************************************************/

void FMediaTextureResource::ConvertResource(const FResource& Resource)
{
	check(IsInRenderingThread());

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	SCOPED_DRAW_EVENT(RHICmdList, MediaTextureConvertResource);

	// configure media shaders
	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	static FGlobalBoundShaderState BoundShaderState;
	TShaderMapRef<FMediaShadersVS> VertexShader(ShaderMap);

	switch (SinkFormat)
	{
	case EMediaTextureSinkFormat::CharAYUV:
		{
			TShaderMapRef<FAYUVConvertPS> ConvertShader(ShaderMap);
			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource);
		}
		break;

	case EMediaTextureSinkFormat::CharNV12:
		{
			TShaderMapRef<FNV12ConvertPS> ConvertShader(ShaderMap);
			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource);
		}
		break;

	case EMediaTextureSinkFormat::CharNV21:
		{
			TShaderMapRef<FNV21ConvertPS> ConvertShader(ShaderMap);
			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource);
		}
		break;

	case EMediaTextureSinkFormat::CharUYVY:
		{
			TShaderMapRef<FUYVYConvertPS> ConvertShader(ShaderMap);
			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource);
		}
		break;

	case EMediaTextureSinkFormat::CharYUY2:
		{
			TShaderMapRef<FYUY2ConvertPS> ConvertShader(ShaderMap);
			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource);
		}
		break;

	case EMediaTextureSinkFormat::CharYVYU:
		{
			TShaderMapRef<FYVYUConvertPS> ConvertShader(ShaderMap);
			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, BoundShaderState, GMediaVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ConvertShader);
			ConvertShader->SetParameters(RHICmdList, Resource.ShaderResource);
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


void FMediaTextureResource::InitializeResource(FIntPoint Dimensions, EMediaTextureSinkFormat Format, EMediaTextureSinkMode Mode)
{
	check(IsInRenderingThread());

	if ((Dimensions == OutputDimensions) && (Format == SinkFormat) && (Mode == SinkMode))
	{
		return; // reuse existing texture
	}

	ReleaseDynamicRHI();
	OutputDimensions = Dimensions;
	SinkFormat = Format;
	SinkMode = Mode;
	InitDynamicRHI();
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
