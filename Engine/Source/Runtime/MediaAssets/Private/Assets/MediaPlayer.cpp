// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPCH.h"
#include "MediaCaptionSink.h"
#include "MediaPlayer.h"
#include "MediaPlaylist.h"
#include "MediaSource.h"
#include "MediaSoundWave.h"
#include "MediaTexture.h"


/* UMediaPlayer structors
 *****************************************************************************/

UMediaPlayer::UMediaPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PlayOnOpen(true)
	, Shuffle(false)
	, Loop(false)
	, PlaylistIndex(INDEX_NONE)
	, CaptionSink(new FMediaCaptionSink)
#if WITH_EDITOR
	, WasPlayingInPIE(false)
#endif
{ }


/* UMediaPlayer interface
 *****************************************************************************/

bool UMediaPlayer::CanPause() const
{
	return Player.IsValid() && (Player->GetControls().GetState() == EMediaState::Playing);
}


void UMediaPlayer::Close()
{
	CurrentUrl.Empty();
	Playlist = nullptr;

	if (Player.IsValid())
	{
		Player->Close();
	}
}


FText UMediaPlayer::GetCaptionText() const
{
	return (CaptionSink != nullptr)
		? FText::FromString(CaptionSink->GetCaption())
		: FText::GetEmpty();
}


FTimespan UMediaPlayer::GetDuration() const
{
	return Player.IsValid() ? Player->GetControls().GetDuration() : FTimespan::Zero();
}


FFloatRange UMediaPlayer::GetForwardRates(bool Unthinned)
{
	return Player.IsValid()
		? Player->GetControls().GetSupportedRates(EMediaPlaybackDirections::Forward, Unthinned)
		: FFloatRange::Empty();
}


int32 UMediaPlayer::GetNumTracks(EMediaPlayerTrack TrackType) const
{
	return Player.IsValid() ? Player->GetTracks().GetNumTracks((EMediaTrackType)TrackType) : 0;
}


FName UMediaPlayer::GetPlayerName() const
{
	return PlayerName;
}


float UMediaPlayer::GetRate() const
{
	return Player.IsValid() ? Player->GetControls().GetRate() : 0.0f;
}


FFloatRange UMediaPlayer::GetReverseRates(bool Unthinned)
{
	return Player.IsValid()
		? Player->GetControls().GetSupportedRates(EMediaPlaybackDirections::Reverse, Unthinned)
		: FFloatRange::Empty();
}


int32 UMediaPlayer::GetSelectedTrack(EMediaPlayerTrack TrackType) const
{
	return Player.IsValid() ? Player->GetTracks().GetSelectedTrack((EMediaTrackType)TrackType) : INDEX_NONE;
}


FTimespan UMediaPlayer::GetTime() const
{
	return Player.IsValid() ? Player->GetControls().GetTime() : FTimespan::Zero();
}


FText UMediaPlayer::GetTrackDisplayName(EMediaPlayerTrack TrackType, int32 TrackIndex) const
{
	return Player.IsValid() ? Player->GetTracks().GetTrackDisplayName((EMediaTrackType)TrackType, TrackIndex) : FText::GetEmpty();
}


FString UMediaPlayer::GetTrackLanguage(EMediaPlayerTrack TrackType, int32 TrackIndex) const
{
	return Player.IsValid() ? Player->GetTracks().GetTrackLanguage((EMediaTrackType)TrackType, TrackIndex) : FString();
}


bool UMediaPlayer::IsLooping() const
{
	return Player.IsValid() && Player->GetControls().IsLooping();
}


bool UMediaPlayer::IsPaused() const
{
	return Player.IsValid() && (Player->GetControls().GetState() == EMediaState::Paused);
}


bool UMediaPlayer::IsPlaying() const
{
	return Player.IsValid() && (Player->GetControls().GetState() == EMediaState::Playing);
}


bool UMediaPlayer::IsPreparing() const
{
	return Player.IsValid() && (Player->GetControls().GetState() == EMediaState::Preparing);
}


bool UMediaPlayer::IsReady() const
{
	if (!Player.IsValid())
	{
		return false;
	}
	
	return ((Player->GetControls().GetState() != EMediaState::Closed) &&
			(Player->GetControls().GetState() != EMediaState::Error) &&
			(Player->GetControls().GetState() != EMediaState::Preparing));
}


bool UMediaPlayer::Next()
{
	if (Playlist == nullptr)
	{
		return false;
	}

	int32 RemainingAttempts = Playlist->Num();

	while (--RemainingAttempts >= 0)
	{
		UMediaSource* NextSource = Shuffle
			? Playlist->GetRandom(PlaylistIndex)
			: Playlist->GetNext(PlaylistIndex);

		if ((NextSource != nullptr) && NextSource->Validate() && Open(NextSource->GetUrl(), *NextSource))
		{
			return true;
		}
	}

	return false;
}


bool UMediaPlayer::OpenPlaylistIndex(UMediaPlaylist* InPlaylist, int32 Index)
{
	if (InPlaylist == nullptr)
	{
		return false;
	}

	Playlist = InPlaylist;
	PlaylistIndex = Index;

	if (PlaylistIndex == INDEX_NONE)
	{
		return true;
	}

	UMediaSource* MediaSource = Playlist->Get(PlaylistIndex);

	if (MediaSource == nullptr)
	{
		return false;
	}
	
	if (!MediaSource->Validate())
	{
		UE_LOG(LogMediaAssets, Error, TEXT("Failed to validate media source %s"), *MediaSource->GetName());

		return false;
	}

	return Open(MediaSource->GetUrl(), *MediaSource);
}


bool UMediaPlayer::OpenSource(UMediaSource* MediaSource)
{
	if (MediaSource == nullptr)
	{
		return false;
	}

	if (!MediaSource->Validate())
	{
		UE_LOG(LogMediaAssets, Error, TEXT("Failed to validate media source %s"), *MediaSource->GetName());

		return false;
	}

	Playlist = nullptr;

	return Open(MediaSource->GetUrl(), *MediaSource);
}


bool UMediaPlayer::OpenUrl(const FString& Url)
{
	Playlist = nullptr;

	return Open(Url, *GetDefault<UMediaSource>());
}


bool UMediaPlayer::Pause()
{
	return SetRate(0.0f);
}


bool UMediaPlayer::Play()
{
	return SetRate(1.0f);
}


bool UMediaPlayer::Previous()
{
	if (Playlist == nullptr)
	{
		return false;
	}

	int32 RemainingAttempts = Playlist->Num();

	while (--RemainingAttempts >= 0)
	{
		UMediaSource* PrevSource = Shuffle
			? Playlist->GetRandom(PlaylistIndex)
			: Playlist->GetNext(PlaylistIndex);

		if ((PrevSource != nullptr) && PrevSource->Validate() && Open(PrevSource->GetUrl(), *PrevSource))
		{
			return true;
		}
	}

	return false;
}


bool UMediaPlayer::Reopen()
{
	if (Playlist != nullptr)
	{
		return OpenPlaylistIndex(Playlist, PlaylistIndex);
	}

	if (!CurrentUrl.IsEmpty())
	{
		OpenUrl(CurrentUrl);
	}

	return false;
}


bool UMediaPlayer::Rewind()
{
	return Seek(FTimespan::Zero());
}


bool UMediaPlayer::Seek(const FTimespan& InTime)
{
	return Player.IsValid() && Player->GetControls().Seek(InTime);
}


bool UMediaPlayer::SelectTrack(EMediaPlayerTrack TrackType, int32 TrackIndex)
{
	return Player.IsValid() ? Player->GetTracks().SelectTrack((EMediaTrackType)TrackType, TrackIndex) : false;
}


void UMediaPlayer::SetImageTexture(UMediaTexture* NewTexture)
{
	if (ImageTexture != nullptr)
	{
		ImageTexture->OnBeginDestroy().RemoveAll(this);
	}

	if (NewTexture != nullptr)
	{
		if (NewTexture == VideoTexture)
		{
			SetVideoTexture(nullptr);
		}

		NewTexture->OnBeginDestroy().AddUObject(this, &UMediaPlayer::HandleMediaTextureBeginDestroy);
	}
	
	if (Player.IsValid())
	{
		Player->GetOutput().SetImageSink(NewTexture);
	}

	ImageTexture = NewTexture;
}


bool UMediaPlayer::SetLooping(bool InLooping)
{
	Loop = InLooping;

	return Player.IsValid() && Player->GetControls().SetLooping(InLooping);
}


bool UMediaPlayer::SetRate(float Rate)
{
	return Player.IsValid() && Player->GetControls().SetRate(Rate);
}


void UMediaPlayer::SetSoundWave(UMediaSoundWave* NewSoundWave)
{
	if (SoundWave != nullptr)
	{
		SoundWave->OnBeginDestroy().RemoveAll(this);
	}

	if (NewSoundWave != nullptr)
	{
		NewSoundWave->OnBeginDestroy().AddUObject(this, &UMediaPlayer::HandleMediaSoundWaveBeginDestroy);
	}

	if (Player.IsValid())
	{
		Player->GetOutput().SetAudioSink(NewSoundWave);
	}

	SoundWave = NewSoundWave;
}


void UMediaPlayer::SetVideoTexture(UMediaTexture* NewTexture)
{
	if (VideoTexture != nullptr)
	{
		VideoTexture->OnBeginDestroy().RemoveAll(this);
	}

	if (NewTexture != nullptr)
	{
		if (NewTexture == ImageTexture)
		{
			SetImageTexture(nullptr);
		}

		NewTexture->OnBeginDestroy().AddUObject(this, &UMediaPlayer::HandleMediaTextureBeginDestroy);
	}

	if (Player.IsValid())
	{
		Player->GetOutput().SetVideoSink(NewTexture);
	}

	VideoTexture = NewTexture;
}


bool UMediaPlayer::SupportsRate(float Rate, bool Unthinned) const
{
	return Player.IsValid() && Player->GetControls().SupportsRate(Rate, Unthinned);
}


bool UMediaPlayer::SupportsScrubbing() const
{
	return Player.IsValid() && Player->GetControls().SupportsScrubbing();
}


bool UMediaPlayer::SupportsSeeking() const
{
	return Player.IsValid() && Player->GetControls().SupportsSeeking();
}


#if WITH_EDITOR

void UMediaPlayer::PausePIE()
{
	WasPlayingInPIE = IsPlaying();

	if (WasPlayingInPIE)
	{
		Pause();
	}
}


void UMediaPlayer::ResumePIE()
{
	if (WasPlayingInPIE)
	{
		Play();
	}
}

#endif


/* UObject overrides
 *****************************************************************************/

void UMediaPlayer::BeginDestroy()
{
	Super::BeginDestroy();

	SetImageTexture(nullptr);
	SetSoundWave(nullptr);
	SetVideoTexture(nullptr);

	if (Player.IsValid())
	{
		Player->Close();
		Player->OnMediaEvent().RemoveAll(this);
		Player.Reset();
	}

	if (CaptionSink != nullptr)
	{
		delete CaptionSink;
		CaptionSink = nullptr;
	}
}


FString UMediaPlayer::GetDesc()
{
	if (Player.IsValid())
	{
		return TEXT("UMediaPlayer");
	}

	return FString();
}


void UMediaPlayer::PostLoad()
{
	Super::PostLoad();

	if (ImageTexture != nullptr)
	{
		ImageTexture->OnBeginDestroy().AddUObject(this, &UMediaPlayer::HandleMediaTextureBeginDestroy);
	}

	if (SoundWave != nullptr)
	{
		SoundWave->OnBeginDestroy().AddUObject(this, &UMediaPlayer::HandleMediaSoundWaveBeginDestroy);
	}

	if (VideoTexture != nullptr)
	{
		VideoTexture->OnBeginDestroy().AddUObject(this, &UMediaPlayer::HandleMediaTextureBeginDestroy);
	}
}


#if WITH_EDITOR

void UMediaPlayer::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMediaPlayer, ImageTexture))
	{
		SetImageTexture(ImageTexture);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMediaPlayer, SoundWave))
	{
		SetSoundWave(SoundWave);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMediaPlayer, VideoTexture))
	{
		SetVideoTexture(VideoTexture);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMediaPlayer, Loop))
	{
		SetLooping(Loop);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UMediaPlayer::PreEditChange(UProperty* PropertyAboutToChange)
{
	const FName PropertyName = (PropertyAboutToChange != nullptr)
		? PropertyAboutToChange->GetFName()
		: NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMediaPlayer, ImageTexture))
	{
		SetImageTexture(nullptr);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMediaPlayer, SoundWave))
	{
		SetSoundWave(nullptr);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMediaPlayer, VideoTexture))
	{
		SetVideoTexture(nullptr);
	}

	Super::PreEditChange(PropertyAboutToChange);
}

#endif


/* UMediaPlayer implementation
 *****************************************************************************/

bool UMediaPlayer::Open(const FString& Url, const IMediaOptions& Options)
{
	if (IsRunningDedicatedServer())
	{
		return false;
	}

	if (Url.IsEmpty())
	{
		return false;
	}

	// load media module
	IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

	if (MediaModule == nullptr)
	{
		UE_LOG(LogMediaAssets, Error, TEXT("Failed to load Media module"));

		return false;
	}

	TSharedPtr<IMediaPlayer> NewPlayer;
	FName NewPlayerName = NAME_None;

	// find a native player
	if (DesiredPlayerName != NAME_None)
	{
		if (DesiredPlayerName != PlayerName)
		{
			// create desired player
			IMediaPlayerFactory* Factory = MediaModule->GetPlayerFactory(DesiredPlayerName);

			if (Factory == nullptr)
			{
				UE_LOG(LogMediaAssets, Error, TEXT("Could not find desired player %s for %s"), *DesiredPlayerName.ToString(), *Url);
			}
			else
			{
				NewPlayer = Factory->CreatePlayer();

				if (!NewPlayer.IsValid())
				{
					UE_LOG(LogMediaAssets, Error, TEXT("Failed to create desired player %s for %s"), *DesiredPlayerName.ToString(), *Url);
				}
				else
				{
					NewPlayerName = DesiredPlayerName;
				}
			}
		}
		else
		{
			NewPlayer = Player; // reuse existing player
		}
	}
	else
	{
		if (PlayerName != NAME_None)
		{
			// try to reuse existing player
			IMediaPlayerFactory* Factory = MediaModule->GetPlayerFactory(PlayerName);

			if ((Factory != nullptr) && Factory->CanPlayUrl(Url, Options))
			{
				NewPlayer = Player;
			}
		}

		if (!NewPlayer.IsValid())
		{
			// automatically select & create new player
			const TArray<IMediaPlayerFactory*>& PlayerFactories = MediaModule->GetPlayerFactories();
			const FString RunningPlatformName(FPlatformProperties::IniPlatformName());

			for (IMediaPlayerFactory* Factory : PlayerFactories)
			{
				if (!Factory->SupportsPlatform(RunningPlatformName) || !Factory->CanPlayUrl(Url, Options))
				{
					continue;
				}

				NewPlayer = Factory->CreatePlayer();

				if (NewPlayer.IsValid())
				{
					NewPlayerName = Factory->GetName();

					break;
				}
			}

			if (!NewPlayer.IsValid())
			{
				UE_LOG(LogMediaAssets, Error, TEXT("Could not find a native player for %s"), *Url);
			}
		}
	}

	// initialize new player
	if (NewPlayer != Player)
	{
		if (Player.IsValid())
		{
			Player->Close();
			Player->OnMediaEvent().RemoveAll(this);
		}

		if (NewPlayer.IsValid())
		{
			NewPlayer->OnMediaEvent().AddUObject(this, &UMediaPlayer::HandlePlayerMediaEvent);
			PlayerName = NewPlayerName;
		}
		else
		{
			PlayerName = NAME_None;
		}

		Player = NewPlayer;
	}

	if (!Player.IsValid())
	{
		return false;
	}

	CurrentUrl = Url;

	// open the new media source
	if (!Player->Open(Url, Options))
	{
		CurrentUrl.Empty();

		return false;
	}

	return true;
}


void UMediaPlayer::SelectDefaultTracks()
{
	IMediaTracks& Tracks = Player->GetTracks();

	// @todo Media: consider locale when selecting default tracks
	Tracks.SelectTrack(EMediaTrackType::Audio, 0);
	Tracks.SelectTrack(EMediaTrackType::Caption, 0);
	Tracks.SelectTrack(EMediaTrackType::Image, 0);
	Tracks.SelectTrack(EMediaTrackType::Video, 0);
}


/* UMediaPlayer callbacks
 *****************************************************************************/

void UMediaPlayer::HandleMediaSoundWaveBeginDestroy(UMediaSoundWave& DestroyedSoundWave)
{
	if (Player.IsValid() && (&DestroyedSoundWave == SoundWave))
	{
		Player->GetOutput().SetAudioSink(nullptr);
	}
}


void UMediaPlayer::HandleMediaTextureBeginDestroy(UMediaTexture& DestroyedMediaTexture)
{
	if (Player.IsValid())
	{
		if (&DestroyedMediaTexture == ImageTexture)
		{
			Player->GetOutput().SetAudioSink(nullptr);
		}
		else if (&DestroyedMediaTexture == VideoTexture)
		{
			Player->GetOutput().SetVideoSink(nullptr);
		}
	}
}


void UMediaPlayer::HandlePlayerMediaEvent(EMediaEvent Event)
{
	MediaEvent.Broadcast(Event);

	switch(Event)
	{
	case EMediaEvent::MediaClosed:
		OnMediaClosed.Broadcast();
		break;

	case EMediaEvent::MediaOpened:
		Player->GetControls().SetLooping(Loop);

		if (ImageTexture != nullptr)
		{
			Player->GetOutput().SetImageSink(ImageTexture);
		}

		if (SoundWave != nullptr)
		{
			Player->GetOutput().SetAudioSink(SoundWave);
		}

		if (VideoTexture != nullptr)
		{
			Player->GetOutput().SetVideoSink(VideoTexture);
		}

		OnMediaOpened.Broadcast(CurrentUrl);

		if (PlayOnOpen)
		{
			Play();
		}
		break;

	case EMediaEvent::MediaOpenFailed:
		OnMediaOpenFailed.Broadcast(CurrentUrl);

		if (!Loop && (Playlist != nullptr))
		{
			Next();
		}
		break;

	case EMediaEvent::PlaybackEndReached:
		OnEndReached.Broadcast();

		if (!Loop && (Playlist != nullptr))
		{
			Next();
		}
		break;

	case EMediaEvent::PlaybackResumed:
		OnPlaybackResumed.Broadcast();
		break;

	case EMediaEvent::PlaybackSuspended:
		OnPlaybackSuspended.Broadcast();
		break;

	case EMediaEvent::TracksChanged:
		SelectDefaultTracks();
		break;
	}

	if ((Event == EMediaEvent::MediaOpened) ||
		(Event == EMediaEvent::MediaOpenFailed))
	{
		UE_LOG(LogMediaAssets, Verbose, TEXT("Media Info:\n\n%s"), *Player->GetInfo());
	}
}
