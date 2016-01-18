// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Texture.h"
#include "MediaTexture.generated.h"


class FMediaSampleBuffer;
class IMediaPlayer;
class IMediaVideoTrack;
class UMediaPlayer;
enum EPixelFormat;
enum TextureAddress;


/**
 * Implements a texture asset for rendering video tracks from UMediaPlayer assets.
 */
UCLASS(hidecategories=(Compression, LevelOfDetail, Object, Texture))
class MEDIAASSETS_API UMediaTexture
	: public UTexture
{
	GENERATED_UCLASS_BODY()

	/** The addressing mode to use for the X axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MediaTexture, AssetRegistrySearchable)
	TEnumAsByte<TextureAddress> AddressX;

	/** The addressing mode to use for the Y axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MediaTexture, AssetRegistrySearchable)
	TEnumAsByte<TextureAddress> AddressY;

	/** The color used to clear the texture if no video data is drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MediaTexture)
	FLinearColor ClearColor;

	/** The index of the MediaPlayer's video track to render on this texture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MediaPlayer)
	int32 VideoTrackIndex;

public:

	/**
	 * Gets the width and height of the texture (in pixels).
	 *
	 * @return Texture dimensions.
	 */
	FIntPoint GetDimensions() const
	{
		return CachedDimensions;
	}

	/**
	 * Gets the texture's pixel format.
	 *
	 * @return Pixel format (always PF_B8G8R8A8 for all movie textures).
	 */
	TEnumAsByte<EPixelFormat> GetFormat() const
	{
		return PF_B8G8R8A8;
	}

	/**
	 * Gets the low-level player associated with the assigned UMediaPlayer asset.
	 *
	 * @return The player, or nullptr if no player is available.
	 */
	TSharedPtr<IMediaPlayer> GetPlayer() const;

	/**
	 * Gets the currently selected video track, if any.
	 *
	 * @return The selected video track, or nullptr if none is selected.
	 */
	TSharedPtr<IMediaVideoTrack, ESPMode::ThreadSafe> GetVideoTrack() const
	{
		return VideoTrack;
	}
	
	/**
	 * Sets the media player asset to be used for this texture.
	 *
	 * @param InMediaPlayer The asset to set.
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaTexture")
	void SetMediaPlayer( UMediaPlayer* InMediaPlayer );

public:

	// UTexture overrides.

	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() override;
	virtual float GetSurfaceWidth() const override;
	virtual float GetSurfaceHeight() const override;
	virtual void UpdateResource() override;	

public:

	// UObject overrides.

	virtual void BeginDestroy() override;
	virtual void FinishDestroy() override;
	virtual FString GetDesc() override;
	virtual SIZE_T GetResourceSize( EResourceSizeMode::Type Mode ) override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PreEditChange( UProperty* PropertyAboutToChange ) override;
	virtual void PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent ) override;
#endif

protected:

	/** The MediaPlayer asset to stream video from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=MediaPlayer)
	UMediaPlayer* MediaPlayer;

protected:

	/** Initializes the video track. */
	void InitializeTrack();

private:

	/** Callback for when the UMediaPlayer changed tracks. */
	void HandleMediaPlayerTracksChanged();

private:

	/** The texture's cached width and height (in pixels). */
	FIntPoint CachedDimensions;

	/** Holds the UMediaPlayer asset currently being used. */
	UPROPERTY() 
	TWeakObjectPtr<UMediaPlayer> CurrentMediaPlayer;

	/** Synchronizes access to this object from the render thread. */
	FRenderCommandFence* ReleasePlayerFence;

	/** The video sample buffer. */
	TSharedRef<FMediaSampleBuffer, ESPMode::ThreadSafe> VideoBuffer;

	/** Holds the selected video track. */
	TSharedPtr<IMediaVideoTrack, ESPMode::ThreadSafe> VideoTrack;

	bool bDelegatesAdded;
};
