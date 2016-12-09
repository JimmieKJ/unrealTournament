// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayer.h"
#include "Modules/ModuleManager.h"
#include "IMediaControls.h"
#include "IMediaModule.h"
#include "IMediaOutput.h"
#include "IMediaPlayer.h"
#include "IMediaPlayerFactory.h"
#include "IMediaTracks.h"
#include "Misc/Paths.h"
#include "MediaAssetsPrivate.h"
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
#if WITH_EDITOR
	, WasPlayingInPIE(false)
#endif
{ }


/* UMediaPlayer interface
 *****************************************************************************/

bool UMediaPlayer::CanPause() const
{
	if (!Player.IsValid())
	{
		return false;
	}
	
	EMediaState State = Player->GetControls().GetState();

	return ((State== EMediaState::Playing) || (State== EMediaState::Preparing));
}


bool UMediaPlayer::CanPlaySource(UMediaSource* MediaSource)
{
	if ((MediaSource == nullptr) || !MediaSource->Validate())
	{
		return false;
	}

	return FindPlayerForUrl(MediaSource->GetUrl(), *MediaSource).IsValid();
}


bool UMediaPlayer::CanPlayUrl(const FString& Url)
{
	if (Url.IsEmpty())
	{
		return false;
	}

	return FindPlayerForUrl(Url, *GetDefault<UMediaSource>()).IsValid();
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


void UMediaPlayer::GetOverlays(EMediaOverlayType Type, TArray<FMediaPlayerOverlay>& OutOverlays) const
{
	FTimespan CurrentTime = GetTime();

	if (CurrentTime == FTimespan::Zero())
	{
		return;
	}

	for (const FOverlay& Overlay : Overlays)
	{
		if ((Overlay.Type == Type) && (Overlay.Time <= CurrentTime) && (CurrentTime - Overlay.Time < Overlay.Duration))
		{
			const int32 OverlayIndex = OutOverlays.AddDefaulted();
			FMediaPlayerOverlay& OutOverlay = OutOverlays[OverlayIndex];
			{
				OutOverlay.HasPosition = Overlay.Position.IsSet();
				OutOverlay.Position = OutOverlay.HasPosition ? Overlay.Position.GetValue() : FIntPoint::ZeroValue;
				OutOverlay.Text = Overlay.Text;
			}
		}
	}
}


FName UMediaPlayer::GetPlayerName() const
{
	return Player.IsValid() ? Player->GetName() : NAME_None;
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

	while (RemainingAttempts-- > 0)
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


bool UMediaPlayer::OpenFile(const FString& FilePath)
{
	FString FullPath;
	
	if (FPaths::IsRelative(FilePath))
	{
		FullPath = FPaths::ConvertRelativePathToFull(FilePath);
	}
	else
	{
		FullPath = FilePath;
		FPaths::NormalizeFilename(FullPath);
	}

	return OpenUrl(FString(TEXT("file://")) + FullPath);
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
		UE_LOG(LogMediaAssets, Error, TEXT("Failed to validate media source %s (%s)"), *MediaSource->GetName(), *MediaSource->GetUrl());

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
		UE_LOG(LogMediaAssets, Error, TEXT("Failed to validate media source %s (%s)"), *MediaSource->GetName(), *MediaSource->GetUrl());

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


bool UMediaPlayer::Seek(const FTimespan& Time)
{
	return Player.IsValid() && Player->GetControls().Seek(Time);
}


bool UMediaPlayer::SelectTrack(EMediaPlayerTrack TrackType, int32 TrackIndex)
{
	return Player.IsValid() ? Player->GetTracks().SelectTrack((EMediaTrackType)TrackType, TrackIndex) : false;
}


bool UMediaPlayer::SetLooping(bool Looping)
{
	Loop = Looping;

	return Player.IsValid() && Player->GetControls().SetLooping(Looping);
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

	SetSoundWave(nullptr);
	SetVideoTexture(nullptr);

	if (Player.IsValid())
	{
		Player->Close();
		Player->GetOutput().SetOverlaySink(nullptr);
		Player->OnMediaEvent().RemoveAll(this);
		Player.Reset();
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

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMediaPlayer, SoundWave))
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

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMediaPlayer, SoundWave))
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

TSharedPtr<IMediaPlayer> UMediaPlayer::FindPlayerForUrl(const FString& Url, const IMediaOptions& Options)
{
	const FName PlayerName = (DesiredPlayerName != NAME_None) ? DesiredPlayerName : Options.GetDesiredPlayerName();

	// reuse existing player if desired
	if (Player.IsValid() && (PlayerName == Player->GetName()))
	{
		return Player;
	}

	// load media module
	IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

	if (MediaModule == nullptr)
	{
		UE_LOG(LogMediaAssets, Error, TEXT("Failed to load Media module"));
		return nullptr;
	}

	// try to create desired player
	if (PlayerName != NAME_None)
	{
		IMediaPlayerFactory* Factory = MediaModule->GetPlayerFactory(PlayerName);

		if (Factory == nullptr)
		{
			UE_LOG(LogMediaAssets, Error, TEXT("Could not find desired player %s for %s"), *PlayerName.ToString(), *Url);
			return nullptr;
		}

		TSharedPtr<IMediaPlayer> NewPlayer = Factory->CreatePlayer();

		if (!NewPlayer.IsValid())
		{
			UE_LOG(LogMediaAssets, Error, TEXT("Failed to create desired player %s for %s"), *PlayerName.ToString(), *Url);
			return nullptr;
		}

		return NewPlayer;
	}

	// try to reuse existing player
	if (Player.IsValid())
	{
		IMediaPlayerFactory* Factory = MediaModule->GetPlayerFactory(Player->GetName());

		if ((Factory != nullptr) && Factory->CanPlayUrl(Url, Options))
		{
			return Player;
		}
	}

	// try to auto-select new player
	const TArray<IMediaPlayerFactory*>& PlayerFactories = MediaModule->GetPlayerFactories();
	const FString RunningPlatformName(FPlatformProperties::IniPlatformName());

	for (IMediaPlayerFactory* Factory : PlayerFactories)
	{
		if (!Factory->SupportsPlatform(RunningPlatformName) || !Factory->CanPlayUrl(Url, Options))
		{
			continue;
		}

		TSharedPtr<IMediaPlayer> NewPlayer = Factory->CreatePlayer();

		if (NewPlayer.IsValid())
		{
			return NewPlayer;
		}
	}

	UE_LOG(LogMediaAssets, Error, TEXT("Could not find a native player for %s"), *Url);

	return nullptr;
}


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

	// find & initialize new player
	TSharedPtr<IMediaPlayer> NewPlayer = FindPlayerForUrl(Url, Options);

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
	Tracks.SelectTrack(EMediaTrackType::Video, 0);
}


/* IMediaOverlaySink interface
 *****************************************************************************/

bool UMediaPlayer::InitializeOverlaySink()
{
	return true;
}


void UMediaPlayer::AddOverlaySinkText(const FText& Text, EMediaOverlayType Type, FTimespan Time, FTimespan Duration, TOptional<FVector2D> Position)
{
	// @todo gmp: make thread-safe

	// remove expired overlays
	const FTimespan CurrentTime = GetTime();

	for (int32 OverlayIndex = Overlays.Num() - 1; OverlayIndex >= 0; --OverlayIndex)
	{
		const FOverlay& Overlay = Overlays[OverlayIndex];
		const FTimespan Offset = CurrentTime - Overlay.Time;

		if (Offset > Overlay.Duration)
		{
			Overlays.RemoveAtSwap(OverlayIndex);
		}
	}

	// add new overlay
	FOverlay Overlay;
	{
		Overlay.Duration = Duration;
		Overlay.Position = Position;
		Overlay.Text = Text;
		Overlay.Time = Time;
		Overlay.Type = Type;
	}

	Overlays.Add(Overlay);
}


void UMediaPlayer::ClearOverlaySinkText()
{
	// @todo gmp: make thread-safe

	Overlays.Empty();
}


void UMediaPlayer::ShutdownOverlaySink()
{
	// @todo gmp: make thread-safe
	Overlays.Empty();
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
	if (Player.IsValid() && (&DestroyedMediaTexture == VideoTexture))
	{
		Player->GetOutput().SetVideoSink(nullptr);
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
		Player->GetOutput().SetOverlaySink(this);

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
