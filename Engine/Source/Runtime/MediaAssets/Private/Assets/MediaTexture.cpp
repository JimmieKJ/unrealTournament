// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPrivatePCH.h"
#include "IMediaVideoTrack.h"
#include "MediaSampleBuffer.h"


/* UMediaTexture structors
 *****************************************************************************/

UMediaTexture::UMediaTexture( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, ClearColor(FLinearColor::Black)
	, VideoTrackIndex(INDEX_NONE)
	, MediaPlayer(nullptr)
	, CurrentMediaPlayer(nullptr)
	, VideoBuffer(MakeShareable(new FMediaSampleBuffer))
{
	bDelegatesAdded = false;
	NeverStream = true;
	UpdateResource();
}


/* UMediaTexture interface
 *****************************************************************************/

TSharedPtr<class IMediaPlayer> UMediaTexture::GetPlayer() const
{
	return (MediaPlayer != nullptr)
		? MediaPlayer->GetPlayer()
		: nullptr;
}


void UMediaTexture::SetMediaPlayer( UMediaPlayer* InMediaPlayer )
{
	MediaPlayer = InMediaPlayer;

	InitializeTrack();
}


/* UTexture  overrides
 *****************************************************************************/

FTextureResource* UMediaTexture::CreateResource()
{
	return new FMediaTextureResource(this, VideoBuffer);
}


EMaterialValueType UMediaTexture::GetMaterialType()
{
	return MCT_Texture2D;
}


float UMediaTexture::GetSurfaceWidth() const
{
	return CachedDimensions.X;
}


float UMediaTexture::GetSurfaceHeight() const
{
	return CachedDimensions.Y;
}

void UMediaTexture::UpdateResource()
{
#if WITH_ENGINE
	if (VideoTrack.IsValid() && Resource)
	{		
		VideoTrack->UnbindTexture(Resource->TextureRHI.GetReference());
	}
#endif
	UTexture::UpdateResource();
}


/* UObject  overrides
 *****************************************************************************/

void UMediaTexture::BeginDestroy()
{
	Super::BeginDestroy();

	// synchronize with the rendering thread by inserting a fence
 	if (!ReleasePlayerFence)
 	{
 		ReleasePlayerFence = new FRenderCommandFence();
 	}

 	ReleasePlayerFence->BeginFence();
}


void UMediaTexture::FinishDestroy()
{
	delete ReleasePlayerFence;
	ReleasePlayerFence = nullptr;

	if (VideoTrack.IsValid())
	{
		VideoTrack->GetStream().RemoveSink(VideoBuffer);
		VideoTrack.Reset();
	}

	if (CurrentMediaPlayer.IsValid())
	{
		CurrentMediaPlayer->OnTracksChanged().RemoveAll(this);
		CurrentMediaPlayer.Reset();
	}

	Super::FinishDestroy();
}


FString UMediaTexture::GetDesc()
{
	TSharedPtr<IMediaPlayer> MediaPlayerPtr = GetPlayer();

	if (!MediaPlayerPtr.IsValid())
	{
		return FString();
	}

	return FString::Printf(TEXT("%dx%d [%s]"), CachedDimensions.X, CachedDimensions.Y, GPixelFormats[GetFormat()].Name);
}


SIZE_T UMediaTexture::GetResourceSize( EResourceSizeMode::Type Mode )
{
	return CachedDimensions.X * CachedDimensions.Y * 4;
}


bool UMediaTexture::IsReadyForFinishDestroy()
{
	// ready to call FinishDestroy if the flushing fence has been hit
	return (Super::IsReadyForFinishDestroy() && ReleasePlayerFence && ReleasePlayerFence->IsFenceComplete());
}


void UMediaTexture::PostLoad()
{
	Super::PostLoad();

	if (!HasAnyFlags(RF_ClassDefaultObject) && !GIsBuildMachine)
	{
		InitializeTrack();
	}
}


#if WITH_EDITOR

void UMediaTexture::PreEditChange( UProperty* PropertyAboutToChange )
{
	// this will release the FMediaTextureResource
	Super::PreEditChange(PropertyAboutToChange);

	FlushRenderingCommands();
}


void UMediaTexture::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	InitializeTrack();

	// this will recreate the FMediaTextureResource
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR


/* UMediaTexture implementation
 *****************************************************************************/

void UMediaTexture::InitializeTrack()
{
	// assign new media player asset
	if ((CurrentMediaPlayer != MediaPlayer) || !bDelegatesAdded)
	{
		if (CurrentMediaPlayer != nullptr)
		{
			CurrentMediaPlayer->OnTracksChanged().RemoveAll(this);
		}

		CurrentMediaPlayer = MediaPlayer;

		if (MediaPlayer != nullptr)
		{
			MediaPlayer->OnTracksChanged().AddUObject(this, &UMediaTexture::HandleMediaPlayerTracksChanged);
		}

		bDelegatesAdded = true;
	}

	// disconnect from current track
	if (VideoTrack.IsValid())
	{
		VideoTrack->GetStream().RemoveSink(VideoBuffer);

#if WITH_ENGINE
		if ((Resource != nullptr) && Resource->TextureRHI.IsValid())
		{
			VideoTrack->UnbindTexture(Resource->TextureRHI.GetReference());
		}
#endif

		VideoTrack.Reset();
	}

	// initialize from new track
	if (MediaPlayer != nullptr)
	{
		IMediaPlayerPtr Player = MediaPlayer->GetPlayer();

		if (Player.IsValid())
		{
			auto VideoTracks = Player->GetVideoTracks();

			if (VideoTracks.IsValidIndex(VideoTrackIndex))
			{
				VideoTrack = VideoTracks[VideoTrackIndex];
			}
			else if (VideoTracks.Num() > 0)
			{
				VideoTrack = VideoTracks[0];
				VideoTrackIndex = 0;
			}
		}
	}

	if (VideoTrack.IsValid())
	{
		CachedDimensions = VideoTrack->GetDimensions();
	}
	else
	{
		CachedDimensions = FIntPoint(ForceInit);
	}

	UpdateResource();

	// connect to new track
	if (VideoTrack.IsValid())
	{
		VideoTrack->GetStream().AddSink(VideoBuffer);

#if WITH_ENGINE
		FlushRenderingCommands();

		if ((Resource != nullptr) && Resource->TextureRHI.IsValid())
		{
			IMediaVideoTrack* VideoTrackPtr = static_cast<IMediaVideoTrack*>(VideoTrack.Get());
			VideoTrackPtr->BindTexture((Resource->TextureRHI.GetReference()));
		}
#endif
	}
}


/* UMediaTexture callbacks
 *****************************************************************************/

void UMediaTexture::HandleMediaPlayerTracksChanged()
{
	InitializeTrack();
}
