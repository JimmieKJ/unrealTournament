// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "UMGSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneBindings.h"
#include "MovieSceneInstance.h"
#include "MovieScene.h"
#include "WidgetAnimation.h"

UUMGSequencePlayer::UUMGSequencePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerStatus = EMovieScenePlayerStatus::Stopped;
	TimeCursorPosition = 0.0f;
	Animation = nullptr;
}

void UUMGSequencePlayer::InitSequencePlayer( const UWidgetAnimation& InAnimation, UUserWidget& UserWidget )
{
	Animation = &InAnimation;

	UMovieScene* MovieScene = Animation->MovieScene;

	// Cache the time range of the sequence to determine when we stop
	TimeRange = MovieScene->GetTimeRange();

	RuntimeBindings = NewObject<UMovieSceneBindings>(this);
	RuntimeBindings->SetRootMovieScene( MovieScene );

	UWidgetTree* WidgetTree = UserWidget.WidgetTree;

	TMap<FGuid, TArray<UObject*> > GuidToRuntimeObjectMap;
	// Bind to Runtime Objects
	for (const FWidgetAnimationBinding& Binding : InAnimation.AnimationBindings)
	{
		UObject* FoundObject = Binding.FindRuntimeObject( *WidgetTree );

		if( FoundObject )
		{
			TArray<UObject*>& Objects = GuidToRuntimeObjectMap.FindOrAdd(Binding.AnimationGuid);
			Objects.Add(FoundObject);
		}
	}

	for( auto It = GuidToRuntimeObjectMap.CreateConstIterator(); It; ++It )
	{
		RuntimeBindings->AddBinding( It.Key(), It.Value() );
	}

}

void UUMGSequencePlayer::Tick(float DeltaTime)
{
	if ( PlayerStatus == EMovieScenePlayerStatus::Playing )
	{
		double LastTimePosition = TimeCursorPosition;

		TimeCursorPosition += bIsPlayingForward ? DeltaTime : -DeltaTime;

		float AnimationLength = TimeRange.GetUpperBoundValue();
		if ( TimeCursorPosition < 0 )
		{
			NumLoopsCompleted++;
			if (NumLoopsToPlay != 0 && NumLoopsCompleted >= NumLoopsToPlay)
			{
				TimeCursorPosition = 0;
				PlayerStatus = EMovieScenePlayerStatus::Stopped;
				OnSequenceFinishedPlayingEvent.Broadcast(*this);
			}
			else
			{
				if (PlayMode == EUMGSequencePlayMode::PingPong)
				{
					bIsPlayingForward = !bIsPlayingForward;
					TimeCursorPosition = FMath::Abs(TimeCursorPosition);
				}
				else
				{
					TimeCursorPosition += AnimationLength;
				}
			}
		}
		else if ( TimeCursorPosition > AnimationLength )
		{
			NumLoopsCompleted++;
			if (NumLoopsToPlay != 0 && NumLoopsCompleted >= NumLoopsToPlay)
			{
				TimeCursorPosition = AnimationLength;
				PlayerStatus = EMovieScenePlayerStatus::Stopped;
				OnSequenceFinishedPlayingEvent.Broadcast(*this);
			}
			else
			{
				if (PlayMode == EUMGSequencePlayMode::PingPong)
				{
					bIsPlayingForward = !bIsPlayingForward;
					TimeCursorPosition = AnimationLength - (TimeCursorPosition - AnimationLength);
				}
				else
				{
					TimeCursorPosition -= AnimationLength;
				}
			}
		}

		if (RootMovieSceneInstance.IsValid())
		{
			RootMovieSceneInstance->Update(TimeCursorPosition, LastTimePosition, *this);
		}
	}
}

void UUMGSequencePlayer::Play(float StartAtTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode)
{
	UMovieScene* MovieScene = Animation->MovieScene;
	RootMovieSceneInstance = MakeShareable( new FMovieSceneInstance( *MovieScene ) );
	RootMovieSceneInstance->RefreshInstance( *this );

	PlayMode = InPlayMode;

	// Clamp the start time to be between 0 and the upper time bound
	TimeCursorPosition = StaticCast<double>(FMath::Clamp(StartAtTime, 0.0f, TimeRange.GetUpperBoundValue()));

	if (PlayMode == EUMGSequencePlayMode::Reverse)
	{
		// When playing in reverse count substract the start time from the end.
		TimeCursorPosition = TimeRange.GetUpperBoundValue() - TimeCursorPosition;
	}

	if ( PlayMode == EUMGSequencePlayMode::PingPong )
	{
		// When animating in ping-pong mode double the number of loops to play so that a loop is a complete forward/reverse cycle.
		NumLoopsToPlay = 2 * InNumLoopsToPlay;
	}
	else
	{
		NumLoopsToPlay = InNumLoopsToPlay;
	}

	NumLoopsCompleted = 0;
	bIsPlayingForward = InPlayMode != EUMGSequencePlayMode::Reverse;

	PlayerStatus = EMovieScenePlayerStatus::Playing;
}

void UUMGSequencePlayer::Pause()
{
	PlayerStatus = EMovieScenePlayerStatus::Stopped;
}

void UUMGSequencePlayer::Stop()
{
	PlayerStatus = EMovieScenePlayerStatus::Stopped;

	OnSequenceFinishedPlayingEvent.Broadcast(*this);

	TimeCursorPosition = 0;
}

void UUMGSequencePlayer::GetRuntimeObjects( TSharedRef<FMovieSceneInstance> MovieSceneInstance, const FGuid& ObjectHandle, TArray< UObject* >& OutObjects ) const
{
	OutObjects = RuntimeBindings->FindBoundObjects( ObjectHandle );
}

EMovieScenePlayerStatus::Type UUMGSequencePlayer::GetPlaybackStatus() const
{
	return PlayerStatus;
}