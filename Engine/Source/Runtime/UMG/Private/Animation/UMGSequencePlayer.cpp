// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "UMGSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneSequenceInstance.h"
#include "MovieScene.h"
#include "WidgetAnimation.h"


UUMGSequencePlayer::UUMGSequencePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerStatus = EMovieScenePlayerStatus::Stopped;
	TimeCursorPosition = 0.0f;
	AnimationStartOffset = 0;
	PlaybackSpeed = 1;
	Animation = nullptr;
}

void UUMGSequencePlayer::InitSequencePlayer( const UWidgetAnimation& InAnimation, UUserWidget& InUserWidget )
{
	Animation = &InAnimation;

	UMovieScene* MovieScene = Animation->GetMovieScene();

	// Cache the time range of the sequence to determine when we stop
	TimeRange = MovieScene->GetPlaybackRange();
	AnimationStartOffset = TimeRange.GetLowerBoundValue();

	UserWidget = &InUserWidget;
	UWidgetTree* WidgetTree = InUserWidget.WidgetTree;

	// Bind to Runtime Objects
	for (const FWidgetAnimationBinding& Binding : InAnimation.GetBindings())
	{
		UObject* FoundObject = Binding.FindRuntimeObject( *WidgetTree , InUserWidget);

		if( FoundObject )
		{
			TArray<UObject*>& Objects = GuidToRuntimeObjectMap.FindOrAdd(Binding.AnimationGuid);
			Objects.Add(FoundObject);
		}
		else
		{
			UE_LOG(LogUMG, Warning, TEXT("Failed to find runtime objects for %s animation, WidgetName: %s, SlotName: %s"), *InAnimation.GetPathName(), *Binding.WidgetName.ToString(), *Binding.SlotWidgetName.ToString() );
		}
	}
}

void UUMGSequencePlayer::Tick(float DeltaTime)
{
	if ( PlayerStatus == EMovieScenePlayerStatus::Playing )
	{
		const double AnimationLength = CurrentPlayRange.Size<double>();

		const double LastTimePosition = TimeCursorPosition;
		TimeCursorPosition += bIsPlayingForward ? DeltaTime * PlaybackSpeed : -DeltaTime * PlaybackSpeed;

		// Check if we crossed over bounds
		const bool bCrossedLowerBound = TimeCursorPosition < CurrentPlayRange.GetLowerBoundValue();
		const bool bCrossedUpperBound = TimeCursorPosition > CurrentPlayRange.GetUpperBoundValue();
		const bool bCrossedEndTime = bIsPlayingForward
			? LastTimePosition < EndTime && EndTime <= TimeCursorPosition
			: LastTimePosition > EndTime && EndTime >= TimeCursorPosition;

		// Increment the num loops if we crossed any bounds.
		if (bCrossedLowerBound || bCrossedUpperBound || (bCrossedEndTime && NumLoopsCompleted >= NumLoopsToPlay - 1))
		{
			NumLoopsCompleted++;
		}

		// Did the animation complete
		const bool bCompleted = NumLoopsToPlay != 0 && NumLoopsCompleted >= NumLoopsToPlay;

		// Handle Times
		if (bCrossedLowerBound)
		{
			if (bCompleted)
			{
				TimeCursorPosition = CurrentPlayRange.GetLowerBoundValue();
			}
			else
			{
				if (PlayMode == EUMGSequencePlayMode::PingPong)
				{
					bIsPlayingForward = !bIsPlayingForward;
					TimeCursorPosition = FMath::Abs(TimeCursorPosition - CurrentPlayRange.GetLowerBoundValue()) + CurrentPlayRange.GetLowerBoundValue();
				}
				else
				{
					TimeCursorPosition += AnimationLength;
				}
			}
		}
		else if (bCrossedUpperBound)
		{
			if (bCompleted)
			{
				TimeCursorPosition = CurrentPlayRange.GetUpperBoundValue();
			}
			else
			{
				if (PlayMode == EUMGSequencePlayMode::PingPong)
				{
					bIsPlayingForward = !bIsPlayingForward;
					TimeCursorPosition = CurrentPlayRange.GetUpperBoundValue() - (TimeCursorPosition - CurrentPlayRange.GetUpperBoundValue());
				}
				else
				{
					TimeCursorPosition -= AnimationLength;
				}
			}
		}
		else if (bCrossedEndTime)
		{
			if (bCompleted)
			{
				TimeCursorPosition = EndTime;
			}
		}
		
		if (bCompleted)
		{
			PlayerStatus = EMovieScenePlayerStatus::Stopped;
			OnSequenceFinishedPlayingEvent.Broadcast(*this);
			Animation->OnAnimationFinished.Broadcast();
		}

		if (RootMovieSceneInstance.IsValid())
		{			
			EMovieSceneUpdateData UpdateData(TimeCursorPosition + AnimationStartOffset, LastTimePosition + AnimationStartOffset);
			RootMovieSceneInstance->Update(UpdateData, *this);
		}
	}
}

void UUMGSequencePlayer::PlayInternal(double StartAtTime, double EndAtTime, double SubAnimStartTime, double SubAnimEndTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode, float InPlaybackSpeed)
{
	RootMovieSceneInstance = MakeShareable( new FMovieSceneSequenceInstance( *Animation ) );
	RootMovieSceneInstance->RefreshInstance( *this );

	PlaybackSpeed = FMath::Abs(InPlaybackSpeed);
	PlayMode = InPlayMode;

	// Set the temporary range for this play of the animation
	CurrentPlayRange = TRange<double>(SubAnimStartTime, TRangeBound<double>::Inclusive(SubAnimEndTime));

	if (PlayMode == EUMGSequencePlayMode::Reverse)
	{
		// When playing in reverse count subtract the start time from the end.
		TimeCursorPosition = CurrentPlayRange.GetUpperBoundValue() - StartAtTime;
	}
	else
	{
		TimeCursorPosition = StartAtTime;
	}
	
	// Clamp the start time and end time to be within the bounds
	TimeCursorPosition = FMath::Clamp(TimeCursorPosition, CurrentPlayRange.GetLowerBoundValue(), CurrentPlayRange.GetUpperBoundValue());
	EndTime = FMath::Clamp(EndAtTime, CurrentPlayRange.GetLowerBoundValue(), CurrentPlayRange.GetUpperBoundValue());

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
	Animation->OnAnimationStarted.Broadcast();
}

void UUMGSequencePlayer::Play(float StartAtTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode, float InPlaybackSpeed)
{
	double SubAnimStartTime = 0.0;
	double SubAnimEndTime = TimeRange.Size<float>();

	PlayInternal(StartAtTime, 0.0, SubAnimStartTime, SubAnimEndTime, InNumLoopsToPlay, InPlayMode, InPlaybackSpeed);
}

void UUMGSequencePlayer::PlayTo(float StartAtTime, float EndAtTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode, float InPlaybackSpeed)
{
	double SubAnimStartTime = 0.0;
	double SubAnimEndTime = TimeRange.Size<float>();

	PlayInternal(StartAtTime, EndAtTime, SubAnimStartTime, SubAnimEndTime, InNumLoopsToPlay, InPlayMode, InPlaybackSpeed);
}

void UUMGSequencePlayer::Pause()
{
	// Purposely don't trigger any OnFinished events
	PlayerStatus = EMovieScenePlayerStatus::Stopped;
}

void UUMGSequencePlayer::Reverse()
{
	if (PlayerStatus == EMovieScenePlayerStatus::Playing)
	{
		bIsPlayingForward = !bIsPlayingForward;
	}
}

void UUMGSequencePlayer::Stop()
{
	PlayerStatus = EMovieScenePlayerStatus::Stopped;

	OnSequenceFinishedPlayingEvent.Broadcast(*this);
	Animation->OnAnimationFinished.Broadcast();

	TimeCursorPosition = 0;
}

void UUMGSequencePlayer::SetNumLoopsToPlay(int32 InNumLoopsToPlay)
{
	if (PlayMode == EUMGSequencePlayMode::PingPong)
	{
		NumLoopsToPlay = (2 * InNumLoopsToPlay);
	}
	else
	{
		NumLoopsToPlay = InNumLoopsToPlay;
	}
}

void UUMGSequencePlayer::SetPlaybackSpeed(float InPlaybackSpeed)
{
	PlaybackSpeed = InPlaybackSpeed;
}

void UUMGSequencePlayer::GetRuntimeObjects(TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance, const FGuid& ObjectHandle, TArray<TWeakObjectPtr<UObject>>& OutObjects) const
{
	const TArray<UObject*>* FoundObjects = GuidToRuntimeObjectMap.Find( ObjectHandle );

	if( FoundObjects )
	{
		OutObjects.Append(*FoundObjects);
	}
}

EMovieScenePlayerStatus::Type UUMGSequencePlayer::GetPlaybackStatus() const
{
	return PlayerStatus;
}

UObject* UUMGSequencePlayer::GetPlaybackContext() const
{
	if (UserWidget.IsValid())
	{
		return UserWidget->GetWorld();
	}
	else
	{
		return nullptr;
	}
}

TArray<UObject*> UUMGSequencePlayer::GetEventContexts() const
{
	TArray<UObject*> EventContexts;
	if (UserWidget.IsValid())
	{
		EventContexts.Add(UserWidget.Get());
	}
	return EventContexts;
}

void UUMGSequencePlayer::SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus)
{
	PlayerStatus = InPlaybackStatus;
}