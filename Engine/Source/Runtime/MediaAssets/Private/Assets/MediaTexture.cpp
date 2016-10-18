// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPCH.h"
#include "MediaTexture.h"
#include "MediaTextureResource.h"


/* UMediaTexture structors
 *****************************************************************************/

UMediaTexture::UMediaTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AddressX(TA_Clamp)
	, AddressY(TA_Clamp)
	, ClearColor(FLinearColor::Black)
	, SinkDimensions(FIntPoint(1, 1))
	, SinkFormat(EMediaTextureSinkFormat::CharBGRA)
	, SinkMode(EMediaTextureSinkMode::Unbuffered)
{
	NeverStream = true;
}


/* FTickerObjectBase interface
 *****************************************************************************/

bool UMediaTexture::Tick(float DeltaTime)
{
	// process deferred tasks
	TFunction<void()> Task;

	while (GameThreadTasks.Dequeue(Task))
	{
		Task();
	}

	return true;
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


bool UMediaTexture::InitializeTextureSink(FIntPoint Dimensions, EMediaTextureSinkFormat Format, EMediaTextureSinkMode Mode)
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("MediaTexture initializing sink with %i x %i pixels %s."), Dimensions.X, Dimensions.Y, (Mode == EMediaTextureSinkMode::Buffered) ? TEXT("Buffered") : TEXT("Unbuffered"));

	SinkDimensions = Dimensions;
	SinkFormat = Format;
	SinkMode = Mode;

	FScopeLock Lock(&CriticalSection);

	if (Resource == nullptr)
	{
		return false;
	}

	((FMediaTextureResource*)Resource)->InitializeBuffer(Dimensions, Format, Mode);

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
	if (ClearColor.A != 0.0f)
	{
		SinkDimensions = FIntPoint(1, 1);
		SinkFormat = EMediaTextureSinkFormat::CharBGRA;
		SinkMode = EMediaTextureSinkMode::Unbuffered;

		GameThreadTasks.Enqueue([=]() {
			UpdateResource();
		});
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
	return new FMediaTextureResource(*this, ClearColor, SinkDimensions, SinkFormat, SinkMode);
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


SIZE_T UMediaTexture::GetResourceSize(EResourceSizeMode::Type Mode)
{
	return (Resource != nullptr) ? ((FMediaTextureResource*)Resource)->GetResourceSize() : 0;
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
