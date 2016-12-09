// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaTexture.h"
#include "Misc/ScopeLock.h"
#include "RenderUtils.h"
#include "MediaAssetsPrivate.h"
#include "Misc/MediaTextureResource.h"


/* UMediaTexture structors
 *****************************************************************************/

UMediaTexture::UMediaTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AddressX(TA_Clamp)
	, AddressY(TA_Clamp)
	, ClearColor(FLinearColor::Black)
	, SinkBufferDim(FIntPoint(1, 1))
	, SinkOutputDim(FIntPoint(1, 1))
	, SinkFormat(EMediaTextureSinkFormat::CharBGRA)
	, SinkMode(EMediaTextureSinkMode::Unbuffered)
{
	NeverStream = true;
}


/* IMediaTextureSink interface
 *****************************************************************************/

float UMediaTexture::GetAspectRatio() const
{
	if (Resource == nullptr)
	{
		return 0.0f;
	}

	const FIntPoint Dimensions = ((FMediaTextureResource*)Resource)->GetSizeXY();

	if (Dimensions.Y == 0)
	{
		return 0.0f;
	}

	return Dimensions.X / Dimensions.Y;
}


int32 UMediaTexture::GetHeight() const
{
	return (Resource != nullptr) ? (int32)Resource->GetSizeY() : 0;
}


int32 UMediaTexture::GetWidth() const
{
	return (Resource != nullptr) ? (int32)Resource->GetSizeX() : 0;
}


/* IMediaTextureSink interface
 *****************************************************************************/

void* UMediaTexture::AcquireTextureSinkBuffer()
{
	FScopeLock Lock(&CriticalSection);

	return (Resource != nullptr) ? ((FMediaTextureResource*)Resource)->AcquireBuffer() : nullptr;
}


void UMediaTexture::DisplayTextureSinkBuffer(FTimespan /*Time*/)
{
	FScopeLock Lock(&CriticalSection);

	if (Resource != nullptr)
	{
		((FMediaTextureResource*)Resource)->DisplayBuffer();
	}
}


FIntPoint UMediaTexture::GetTextureSinkDimensions() const
{
	FScopeLock Lock(&CriticalSection);

	return (Resource != nullptr) ? ((FMediaTextureResource*)Resource)->GetSizeXY() : FIntPoint::ZeroValue;
}


EMediaTextureSinkFormat UMediaTexture::GetTextureSinkFormat() const
{
	return SinkFormat;
}


EMediaTextureSinkMode UMediaTexture::GetTextureSinkMode() const
{
	return SinkMode;
}


FRHITexture* UMediaTexture::GetTextureSinkTexture()
{
	FScopeLock Lock(&CriticalSection);

	return (Resource != nullptr) ? ((FMediaTextureResource*)Resource)->GetTexture() : nullptr;
}


bool UMediaTexture::InitializeTextureSink(FIntPoint OutputDim, FIntPoint BufferDim, EMediaTextureSinkFormat Format, EMediaTextureSinkMode Mode)
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("MediaTexture initializing sink with %ix%i output and %ix%i buffer as %s."), OutputDim.X, OutputDim.Y, BufferDim.X, BufferDim.Y, (Mode == EMediaTextureSinkMode::Buffered) ? TEXT("Buffered") : TEXT("Unbuffered"));

	SinkBufferDim = BufferDim;
	SinkOutputDim = OutputDim;
	SinkFormat = Format;
	SinkMode = Mode;

	FScopeLock Lock(&CriticalSection);

	if (Resource == nullptr)
	{
		return false;
	}

	((FMediaTextureResource*)Resource)->InitializeBuffer(OutputDim, BufferDim, Format, Mode);

	return true;
}


void UMediaTexture::ReleaseTextureSinkBuffer()
{
	FScopeLock Lock(&CriticalSection);

	if (Resource != nullptr)
	{
		((FMediaTextureResource*)Resource)->ReleaseBuffer();
	}
}


void UMediaTexture::ShutdownTextureSink()
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("MediaTexture shutting down sink."));

	if (ClearColor.A == 0.0f)
	{
		return;
	}

	// reset to 1x1 clear color
	SinkBufferDim = FIntPoint(1, 1);
	SinkOutputDim = FIntPoint(1, 1);
	SinkFormat = EMediaTextureSinkFormat::CharBGRA;
	SinkMode = EMediaTextureSinkMode::Unbuffered;

	FScopeLock Lock(&CriticalSection);

	if (Resource != nullptr)
	{
		((FMediaTextureResource*)Resource)->InitializeBuffer(SinkOutputDim, SinkBufferDim, SinkFormat, SinkMode);
	}
}


bool UMediaTexture::SupportsTextureSinkFormat(EMediaTextureSinkFormat Format) const
{
	return true; // all formats are supported
}


void UMediaTexture::UpdateTextureSinkBuffer(const uint8* Data, uint32 Pitch)
{
	FScopeLock Lock(&CriticalSection);

	if (Resource != nullptr)
	{
		((FMediaTextureResource*)Resource)->UpdateBuffer(Data, Pitch);
	}
}


void UMediaTexture::UpdateTextureSinkResource(FRHITexture* RenderTarget, FRHITexture* ShaderResource)
{
	FScopeLock Lock(&CriticalSection);

	if (Resource != nullptr)
	{
		((FMediaTextureResource*)Resource)->UpdateTextures(RenderTarget, ShaderResource);
	}
}


/* UTexture interface
 *****************************************************************************/

FTextureResource* UMediaTexture::CreateResource()
{
	return new FMediaTextureResource(*this, ClearColor, SinkOutputDim, SinkFormat, SinkMode);
}


EMaterialValueType UMediaTexture::GetMaterialType()
{
	return MCT_Texture2D;
}


float UMediaTexture::GetSurfaceWidth() const
{
	return (Resource != nullptr) ? Resource->GetSizeX() : 0.0f;
}


float UMediaTexture::GetSurfaceHeight() const
{
	return (Resource != nullptr) ? Resource->GetSizeY() : 0.0f;
}


void UMediaTexture::UpdateResource()
{
	FScopeLock Lock(&CriticalSection);

	Super::UpdateResource();
}


/* UObject interface
 *****************************************************************************/

void UMediaTexture::BeginDestroy()
{
	Super::BeginDestroy();

	BeginDestroyEvent.Broadcast(*this);
}


FString UMediaTexture::GetDesc()
{
	return FString::Printf(TEXT("%dx%d [%s]"), GetSurfaceWidth(),  GetSurfaceHeight(), GPixelFormats[PF_B8G8R8A8].Name);
}


void UMediaTexture::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	if (Resource)
	{
		((FMediaTextureResource*)Resource)->GetResourceSizeEx(CumulativeResourceSize);
	}
}


#if WITH_EDITOR

void UMediaTexture::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UMediaTexture, ClearColor)) &&
		(PropertyChangedEvent.ChangeType != EPropertyChangeType::ValueSet))
	{
		UObject::PostEditChangeProperty(PropertyChangedEvent);
	}
	else
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
	}
}


void UMediaTexture::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	FlushRenderingCommands();
}

#endif // WITH_EDITOR
