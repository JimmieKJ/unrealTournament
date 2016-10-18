// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "SubtitleManager.h"
#include "SoundDefinitions.h"
#include "Engine/Font.h"
#include "AudioThread.h"

DEFINE_LOG_CATEGORY(LogSubtitle);

// Modifier to the spacing between lines in subtitles
#define MULTILINE_SPACING_SCALING 1.1f

/**
 * Kills all the subtitles
 */
void FSubtitleManager::KillAllSubtitles( void )
{
	ActiveSubtitles.Empty();
}

/**
 * Kills all the subtitles associated with the subtitle ID.
 * This is called from AudioComponent::Stop
 */
void FSubtitleManager::KillSubtitles( PTRINT SubtitleID )
{
	ActiveSubtitles.Remove( SubtitleID );
}

void FSubtitleManager::QueueSubtitles(const FQueueSubtitleParams& QueueSubtitleParams)
{
	check(IsInAudioThread());

	DECLARE_CYCLE_STAT(TEXT("FGameThreadAudioTask.QueueSubtitles"), STAT_AudioQueueSubtitles, STATGROUP_TaskGraphTasks);

	FAudioThread::RunCommandOnGameThread([QueueSubtitleParams]()
	{
		UAudioComponent* AudioComponent = UAudioComponent::GetAudioComponentFromID(QueueSubtitleParams.AudioComponentID);
		if (AudioComponent && AudioComponent->OnQueueSubtitles.IsBound())
		{
			// intercept the subtitles if the delegate is set
			AudioComponent->OnQueueSubtitles.ExecuteIfBound(QueueSubtitleParams.Subtitles, QueueSubtitleParams.Duration);
		}
		else if (UWorld* World = QueueSubtitleParams.WorldPtr.Get())
		{
			// otherwise, pass them on to the subtitle manager for display
			// Subtitles are hashed based on the associated sound (wave instance).
			FSubtitleManager::GetSubtitleManager()->QueueSubtitles( QueueSubtitleParams.WaveInstance, QueueSubtitleParams.SubtitlePriority, QueueSubtitleParams.bManualWordWrap, QueueSubtitleParams.bSingleLine, QueueSubtitleParams.Duration, QueueSubtitleParams.Subtitles, World->GetAudioTimeSeconds() );
		}
	}, GET_STATID(STAT_AudioQueueSubtitles));
}

/**
 * Add an array of subtitles to the active list
 * This is called from AudioComponent::Play
 * As there is only a global subtitle manager, this system will not have different subtitles for different viewports
 *
 * @param  SubTitleID - the controlling id that groups sets of subtitles together
 * @param  Priority - used to prioritize subtitles; higher values have higher priority.  Subtitles with a priority 0.0f are not displayed.
 * @param  StartTime - float of seconds that the subtitles start at
 * @param  SoundDuration - time in seconds after which the subtitles do not display
 * @param  Subtitles - TArray of lines of subtitle and time offset to play them
 */
void FSubtitleManager::QueueSubtitles( PTRINT SubtitleID, float Priority, bool bManualWordWrap, bool bSingleLine, float SoundDuration, const TArray<FSubtitleCue>& Subtitles, float InStartTime )
{
	check( GEngine );
	check(IsInGameThread());

	// No subtitles to display
	if( !Subtitles.Num() )
	{
		return;
	}

	// Return when a subtitle is played with its subtitle suppressed.
	if (Priority == 0.0f)
	{
		return;
	}

	// No sound associated with the subtitle (used for automatic timing)
	if( SoundDuration == 0.0f )
	{
		UE_LOG(LogSubtitle, Warning, TEXT( "Received subtitle with no sound duration." ) );
		return;
	}

	// Add in the new subtitles
	FActiveSubtitle& NewSubtitle = ActiveSubtitles.Add( SubtitleID, FActiveSubtitle( 0, Priority, bManualWordWrap, bSingleLine, Subtitles ) );

	// Resolve time offsets to absolute time
	for( FSubtitleCue& Subtitle : NewSubtitle.Subtitles )
	{
		if (Subtitle.Time > SoundDuration)
		{
			Subtitle.Time = SoundDuration;
			UE_LOG(LogSubtitle, Warning, TEXT( "Subtitle has time offset greater than length of sound - clamping" ) );
		}

		Subtitle.Time += InStartTime;
	}

	// Add on a blank at the end to clear
	FSubtitleCue& Temp = NewSubtitle.Subtitles[NewSubtitle.Subtitles.AddDefaulted()];
	Temp.Text = FText::GetEmpty();
	Temp.Time = InStartTime + SoundDuration;
}

/**
 * Draws a subtitle at the specified pixel location.
 */
#define SUBTITLE_CHAR_HEIGHT		24
/** The default offset of the outline box */
FIntRect DrawStringOutlineBoxOffset(2, 2, 4, 4);

void FSubtitleManager::DisplaySubtitle( FCanvas* Canvas, FActiveSubtitle* Subtitle, FIntRect& Parms, const FLinearColor& Color )
{
	// These should be valid in here
	check( GEngine );
	check( Canvas );

	CurrentSubtitleHeight = 0.0f;

	// This can be NULL when there's an asset mixup (especially with localisation)
	if( !GEngine->GetSubtitleFont() )
	{
		UE_LOG(LogSubtitle, Warning, TEXT( "NULL GEngine->GetSubtitleFont() - subtitles not rendering!" ) );
		return;
	}

	float FontHeight = GEngine->GetSubtitleFont()->GetMaxCharHeight();
	float HeightTest = Canvas->GetRenderTarget()->GetSizeXY().Y;
	int32 SubtitleHeight = FMath::TruncToInt( ( FontHeight * MULTILINE_SPACING_SCALING ) );
	FIntRect BackgroundBoxOffset = DrawStringOutlineBoxOffset;

	// Needed to add a drop shadow and doing all 4 sides was the only way to make them look correct.  If this shows up as a framerate hit we'll have to think of a different way of dealing with this.
	if( Subtitle->bSingleLine )
	{
		const FText& SubtitleText = Subtitle->Subtitles[Subtitle->Index].Text;
		if ( !SubtitleText.IsEmpty() )
		{
			
			// Display lines up from the bottom of the region
			Parms.Max.Y -= SUBTITLE_CHAR_HEIGHT;
			
			FCanvasTextItem TextItem( FVector2D( Parms.Min.X + ( Parms.Width() / 2 ), Parms.Max.Y ), SubtitleText , GEngine->GetSubtitleFont(), Color );
			TextItem.Depth = SUBTITLE_SCREEN_DEPTH_FOR_3D;
			TextItem.bOutlined = true;
			TextItem.bCentreX = true;
			TextItem.OutlineColor = FLinearColor::Black;
			Canvas->DrawItem( TextItem );
			
			CurrentSubtitleHeight += SubtitleHeight;
		}
	}
	else
	{
		for( int32 Idx = Subtitle->Subtitles.Num() - 1; Idx >= 0; Idx-- )
		{
			const FText& SubtitleText = Subtitle->Subtitles[Idx].Text;
			// Display lines up from the bottom of the region
			if ( !SubtitleText.IsEmpty() )
			{
				FCanvasTextItem TextItem( FVector2D( Parms.Min.X + ( Parms.Width() / 2 ), Parms.Max.Y ), SubtitleText, GEngine->GetSubtitleFont(), Color );
				TextItem.Depth = SUBTITLE_SCREEN_DEPTH_FOR_3D;
				TextItem.bOutlined = true;
				TextItem.bCentreX = true;
				TextItem.OutlineColor = FLinearColor::Black;
				Canvas->DrawItem( TextItem );

				Parms.Max.Y -= SubtitleHeight;
				CurrentSubtitleHeight += SubtitleHeight;
				// Don't overlap subsequent boxes...
				BackgroundBoxOffset.Max.Y = BackgroundBoxOffset.Min.Y;
			}
		}
	}
}

/**
 * If any of the active subtitles need to be split into multiple lines, do so now
 * - caveat - this assumes the width of the subtitle region does not change while the subtitle is active
 */
void FSubtitleManager::SplitLinesToSafeZone( FCanvas* Canvas, FIntRect& SubtitleRegion )
{
	int32				i;
	FString			Concatenated;
	float			SoundDuration;
	float			StartTime, SecondsPerChar, Cumulative;

	for( TMap<PTRINT, FActiveSubtitle>::TIterator It( ActiveSubtitles ); It; ++It )
	{
		FActiveSubtitle& Subtitle = It.Value();
		if( Subtitle.bSplit )
		{
			continue;
		}

		// Concatenate the lines into one (in case the lines were partially manually split)
		Concatenated.Empty( 256 );

		// Set up the base data
		FSubtitleCue& Initial = Subtitle.Subtitles[ 0 ];
		Concatenated = Initial.Text.ToString();
		StartTime = Initial.Time;
		SoundDuration = 0.0f;
		for( i = 1; i < Subtitle.Subtitles.Num(); i++ )
		{
			FSubtitleCue& Subsequent = Subtitle.Subtitles[ i ];
			Concatenated += Subsequent.Text.ToString();
			// Last blank entry sets the cutoff time to the duration of the sound
			SoundDuration = Subsequent.Time - StartTime;
		}

		// Adjust concat string to use \n character instead of "/n" or "\n"
		int32 SubIdx = Concatenated.Find( TEXT( "/n" ) );
		while( SubIdx >= 0 )
		{
			Concatenated = Concatenated.Left( SubIdx ) + ( ( TCHAR )L'\n' ) + Concatenated.Right( Concatenated.Len() - ( SubIdx + 2 ) );
			SubIdx = Concatenated.Find( TEXT( "/n" ) );
		}

		SubIdx = Concatenated.Find( TEXT( "\\n" ) );
		while( SubIdx >= 0 )
		{
			Concatenated = Concatenated.Left( SubIdx ) + ( ( TCHAR )L'\n' ) + Concatenated.Right( Concatenated.Len() - ( SubIdx + 2 ) );
			SubIdx = Concatenated.Find( TEXT( "\\n" ));
		}

		// Work out a metric for the length of time a line should be displayed
		SecondsPerChar = SoundDuration / Concatenated.Len();

		// Word wrap into lines
		TArray<FWrappedStringElement> Lines;
		FTextSizingParameters RenderParms( 0.0f, 0.0f, SubtitleRegion.Width(), 0.0f, GEngine->GetSubtitleFont() );
		Canvas->WrapString( RenderParms, 0, *Concatenated, Lines );

		// Set up the times
		Subtitle.Subtitles.Empty();
		Cumulative = 0.0f;
		for( i = 0; i < Lines.Num(); i++ )
		{
			FSubtitleCue& Line = Subtitle.Subtitles[Subtitle.Subtitles.AddDefaulted()];
			
			Line.Text = FText::FromString(Lines[ i ].Value);
			Line.Time = StartTime + Cumulative;

			Cumulative += SecondsPerChar * Line.Text.ToString().Len();
		}

		// Add in the blank terminating line
		FSubtitleCue& Temp = Subtitle.Subtitles[Subtitle.Subtitles.AddDefaulted()];
		Temp.Text = FText::GetEmpty();
		Temp.Time = StartTime + SoundDuration;

		UE_LOG(LogAudio, Log, TEXT( "Splitting subtitle:" ) );
		for( i = 0; i < Subtitle.Subtitles.Num() - 1; i++ )
		{
			FSubtitleCue& Cue = Subtitle.Subtitles[ i ];
			FSubtitleCue& NextCue = Subtitle.Subtitles[ i + 1 ];
			UE_LOG(LogAudio, Log, TEXT( " ... '%s' at %g to %g" ), *(Cue.Text.ToString()), Cue.Time, NextCue.Time );
		}

		// Mark it as split so it doesn't happen again
		Subtitle.bSplit = true;
	}
}

/**
 * Trim the SubtitleRegion to the safe 80% of the canvas 
 */
void FSubtitleManager::TrimRegionToSafeZone( FCanvas* Canvas, FIntRect& SubtitleRegion )
{
	FIntRect SafeZone;

	// Must display all text within text safe area - currently defined as 80% of the screen width and height
	SafeZone.Min.X = ( 10 * Canvas->GetRenderTarget()->GetSizeXY().X ) / 100;
	SafeZone.Min.Y = ( 10 * Canvas->GetRenderTarget()->GetSizeXY().Y ) / 100;
	SafeZone.Max.X = Canvas->GetRenderTarget()->GetSizeXY().X - SafeZone.Min.X;
	SafeZone.Max.Y = Canvas->GetRenderTarget()->GetSizeXY().Y - SafeZone.Min.Y;

	// Trim to safe area, but keep everything central
	if( SubtitleRegion.Min.X < SafeZone.Min.X || SubtitleRegion.Max.X > SafeZone.Max.X )
	{
		int32 Delta = FMath::Max( SafeZone.Min.X - SubtitleRegion.Min.X, SubtitleRegion.Max.X - SafeZone.Max.X );
		SubtitleRegion.Min.X += Delta;
		SubtitleRegion.Max.X -= Delta;
	}

	if( SubtitleRegion.Max.Y > SafeZone.Max.Y )
	{
		SubtitleRegion.Max.Y = SafeZone.Max.Y;
	}
}

/**
 * Find the highest priority subtitle from the list of currently active ones
 */
PTRINT FSubtitleManager::FindHighestPrioritySubtitle( float CurrentTime )
{
	// Tick the available subtitles and find the highest priority one
	float HighestPriority = -1.0f;
	PTRINT HighestPriorityID = 0;

	for( TMap<PTRINT, FActiveSubtitle>::TIterator It( ActiveSubtitles ); It; ++It )
	{
		FActiveSubtitle& Subtitle = It.Value();
		
		// Remove when last entry is reached
		if( Subtitle.Index == Subtitle.Subtitles.Num() - 1 )
		{
			It.RemoveCurrent();
			continue;
		}

		if( CurrentTime >= Subtitle.Subtitles[ Subtitle.Index + 1 ].Time )
		{
			Subtitle.Index++;
		}

		if( Subtitle.Priority > HighestPriority )
		{
			HighestPriority = Subtitle.Priority;
			HighestPriorityID = It.Key();
		}
	}

	return( HighestPriorityID );
}

/**
 * Display the currently queued subtitles and cleanup after they have finished rendering
 *
 * @param  Canvas - where to render the subtitles
 * @param  CurrentTime - current world time
 */
void FSubtitleManager::DisplaySubtitles( FCanvas* InCanvas, FIntRect& InSubtitleRegion, float InAudioTimeSeconds )
{
	check( GEngine );
	check( InCanvas );

	// Do nothing if subtitles are disabled.
	if( !GEngine->bSubtitlesEnabled )
	{
		return;
	}

	if( !GEngine->GetSubtitleFont() )
	{
		UE_LOG(LogSubtitle, Warning, TEXT( "NULL GEngine->GetSubtitleFont() - subtitles not rendering!" ) );
		return;
	}
	
	if (InSubtitleRegion.Area() > 0)
	{
		// Work out the safe zones
		TrimRegionToSafeZone( InCanvas, InSubtitleRegion );

		// If the lines have not already been split, split them to the safe zone now
		SplitLinesToSafeZone( InCanvas, InSubtitleRegion );

		// Find the subtitle to display
		PTRINT HighestPriorityID = FindHighestPrioritySubtitle( InAudioTimeSeconds );

		// Display the highest priority subtitle
		if( HighestPriorityID )
		{
			FActiveSubtitle* Subtitle = ActiveSubtitles.Find( HighestPriorityID );
			DisplaySubtitle( InCanvas, Subtitle, InSubtitleRegion, FLinearColor::White );
		}
		else
		{
			CurrentSubtitleHeight = 0.0f;
		}
	}
}

/**
 * Returns the singleton subtitle manager instance, creating it if necessary.
 */
FSubtitleManager* FSubtitleManager::GetSubtitleManager( void )
{
	static FSubtitleManager* SSubtitleManager = nullptr;

	if( !SSubtitleManager )
	{
		SSubtitleManager = new FSubtitleManager();
		SSubtitleManager->CurrentSubtitleHeight = 0.0f;
	}

	return( SSubtitleManager );
}

// end
