// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Engine/Texture.h"
#include "IMediaTextureSink.h"
#include "MediaTexture.generated.h"

class FTextureResource;

/**
 * Implements a texture asset for rendering video tracks from UMediaPlayer assets.
 */
UCLASS(hidecategories=(Compression, LevelOfDetail, Object, Texture))
class MEDIAASSETS_API UMediaTexture
	: public UTexture
	, public IMediaTextureSink
{
	GENERATED_UCLASS_BODY()

	/** The addressing mode to use for the X axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media|MediaTexture", AssetRegistrySearchable)
	TEnumAsByte<TextureAddress> AddressX;

	/** The addressing mode to use for the Y axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media|MediaTexture", AssetRegistrySearchable)
	TEnumAsByte<TextureAddress> AddressY;

	/** The color used to clear the texture if CloseAction is set to Clear (default = black). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media|MediaTexture", AdvancedDisplay)
	FLinearColor ClearColor;

public:

	/**
	 * Gets the current aspect ratio of the texture.
	 *
	 * @return Texture aspect ratio.
	 * @see GetHeight, GetWidth
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaTexture")
	float GetAspectRatio() const;

	/**
	 * Gets the current height of the texture.
	 *
	 * @return Texture height (in pixels).
	 * @see GetAspectRatio, GetWidth
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaTexture")
	int32 GetHeight() const;

	/**
	 * Gets the current width of the texture.
	 *
	 * @return Texture width (in pixels).
	 * @see GetAspectRatio, GetHeight
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaTexture")
	int32 GetWidth() const;

public:

	DECLARE_EVENT_OneParam(UMediaTexture, FOnBeginDestroy, UMediaTexture& /*DestroyedMediaTexture*/)
	FOnBeginDestroy& OnBeginDestroy()
	{
		return BeginDestroyEvent;
	}

public:

	//~ IMediaTextureSink interface

	virtual void* AcquireTextureSinkBuffer() override;
	virtual void DisplayTextureSinkBuffer(FTimespan Time) override;
	virtual FIntPoint GetTextureSinkDimensions() const override;
	virtual EMediaTextureSinkFormat GetTextureSinkFormat() const override;
	virtual EMediaTextureSinkMode GetTextureSinkMode() const override;
	virtual FRHITexture* GetTextureSinkTexture() override;
	virtual bool InitializeTextureSink(FIntPoint OutputDim, FIntPoint BufferDim, EMediaTextureSinkFormat Format, EMediaTextureSinkMode Mode) override;
	virtual void ReleaseTextureSinkBuffer() override;
	virtual void ShutdownTextureSink() override;
	virtual bool SupportsTextureSinkFormat(EMediaTextureSinkFormat Format) const override;
	virtual void UpdateTextureSinkBuffer(const uint8* Data, uint32 Pitch = 0) override;
	virtual void UpdateTextureSinkResource(FRHITexture* RenderTarget, FRHITexture* ShaderResource) override;

public:

	//~ UTexture interface.

	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() override;
	virtual float GetSurfaceWidth() const override;
	virtual float GetSurfaceHeight() const override;
	virtual void UpdateResource() override;

public:

	//~ UObject interface.

	virtual void BeginDestroy() override;
	virtual FString GetDesc() override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
#endif

protected:

	//~ Deprecated members

	DEPRECATED_FORGAME(4.13, "The MediaPlayer property is no longer used. Please upgrade your content to Media Framework 2.0")
	UPROPERTY(BlueprintReadWrite, Category=MediaPlayer)
	class UMediaPlayer* MediaPlayer;
	
	DEPRECATED_FORGAME(4.13, "The VideoTrackIndex property is no longer used. Please upgrade your content to Media Framework 2.0")
	UPROPERTY(BlueprintReadWrite, Category=MediaPlayer)
	int32 VideoTrackIndex;

private:

	/** An event delegate that is invoked when this media texture is being destroyed. */
	FOnBeginDestroy BeginDestroyEvent;

	/** Critical section for synchronizing access to texture resource object. */
	mutable FCriticalSection CriticalSection;

	/** Width and height of the sink buffers (in pixels). */
	FIntPoint SinkBufferDim;

	/** Width and height of the video output (in pixels). */
	FIntPoint SinkOutputDim;

	/** The render target's pixel format. */
	EMediaTextureSinkFormat SinkFormat;

	/** The mode that this sink is currently operating in. */
	EMediaTextureSinkMode SinkMode;
};
