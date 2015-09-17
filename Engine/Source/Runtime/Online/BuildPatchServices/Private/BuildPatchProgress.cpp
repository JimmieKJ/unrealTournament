// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchProgress.cpp: Implements classes involved with tracking the patch
	progress information.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"

#define LOCTEXT_NAMESPACE "BuildPatchInstallProgress"

/* EBuildPatchProgress implementation
*****************************************************************************/
const FString& EBuildPatchProgress::ToString(const EBuildPatchProgress::Type& ProgressValue)
{
	// Static const fixed FString values so that they are not constantly constructed
	static const FString Initializing(TEXT("Initialising"));
	static const FString Resuming(TEXT("Resuming"));
	static const FString Downloading(TEXT("Downloading"));
	static const FString Recycling(TEXT("Recycling"));
	static const FString Installing(TEXT("Installing"));
	static const FString MovingToInstall(TEXT("MovingToInstall"));
	static const FString SettingAttributes(TEXT("SettingAttributes"));
	static const FString BuildVerification(TEXT("BuildVerification"));
	static const FString CleanUp(TEXT("CleanUp"));
	static const FString PrerequisitesInstall(TEXT("PrerequisitesInstall"));
	static const FString Completed(TEXT("Completed"));
	static const FString Error(TEXT("Error"));
	static const FString Paused(TEXT("Paused"));
	static const FString Default(TEXT("InvalidOrMax"));

	switch (ProgressValue)
	{
		case EBuildPatchProgress::Initializing:
			return Initializing;
		case EBuildPatchProgress::Resuming:
			return Resuming;
		case EBuildPatchProgress::Downloading:
			return Downloading;
		case EBuildPatchProgress::Installing:
			return Installing;
		case EBuildPatchProgress::MovingToInstall:
			return MovingToInstall;
		case EBuildPatchProgress::SettingAttributes:
			return SettingAttributes;
		case EBuildPatchProgress::BuildVerification:
			return BuildVerification;
		case EBuildPatchProgress::CleanUp:
			return CleanUp;
		case EBuildPatchProgress::PrerequisitesInstall:
			return PrerequisitesInstall;
		case EBuildPatchProgress::Completed:
			return Completed;
		case EBuildPatchProgress::Error:
			return Error;
		case EBuildPatchProgress::Paused:
			return Paused;
		default:
			return Default;
	}
}

const FText& EBuildPatchProgress::ToText(const EBuildPatchProgress::Type& ProgressType)
{
	// Static const fixed FText values so that they are not constantly constructed
	static const FText Initializing = LOCTEXT("EBuildPatchProgress_Initialising", "Initializing");
	static const FText Resuming = LOCTEXT("EBuildPatchProgress_Resuming", "Resuming");
	static const FText Downloading = LOCTEXT("EBuildPatchProgress_Downloading", "Downloading");
	static const FText Installing = LOCTEXT("EBuildPatchProgress_Installing", "Installing");
	static const FText BuildVerification = LOCTEXT("EBuildPatchProgress_BuildVerification", "Verifying");
	static const FText CleanUp = LOCTEXT("EBuildPatchProgress_CleanUp", "Cleaning up");
	static const FText PrerequisitesInstall = LOCTEXT("EBuildPatchProgress_PrerequisitesInstall", "Installing Prerequisites");
	static const FText Completed = LOCTEXT("EBuildPatchProgress_Complete", "Complete");
	static const FText Error = LOCTEXT("EBuildPatchProgress_Error", "Error");
	static const FText Paused = LOCTEXT("EBuildPatchProgress_Paused", "Paused");
	static const FText Empty = FText::GetEmpty();

	switch (ProgressType)
	{
		case EBuildPatchProgress::Initializing:
			return Initializing;
		case EBuildPatchProgress::Resuming:
			return Resuming;
		case EBuildPatchProgress::Downloading:
			return Downloading;
		case EBuildPatchProgress::Installing:
		case EBuildPatchProgress::MovingToInstall:
		case EBuildPatchProgress::SettingAttributes:
			return Installing;
		case EBuildPatchProgress::BuildVerification:
			return BuildVerification;
		case EBuildPatchProgress::CleanUp:
			return CleanUp;
		case EBuildPatchProgress::PrerequisitesInstall:
			return PrerequisitesInstall;
		case EBuildPatchProgress::Completed:
			return Completed;
		case EBuildPatchProgress::Error:
			return Error;
		case EBuildPatchProgress::Paused:
			return Paused;
		default:
			return Empty;
	}
}

/* FBuildPatchProgress implementation
*****************************************************************************/
FBuildPatchProgress::FBuildPatchProgress()
{
	Reset();
}

void FBuildPatchProgress::Reset()
{
	TotalWeight = 0.0f;
	CurrentState = EBuildPatchProgress::Initializing;
	CurrentProgress = 0.0f;
	ErrorText = FText::GetEmpty();
	ShortErrorText = FText::GetEmpty();

	// Initialize array data
	for( uint32 idx = 0; idx < EBuildPatchProgress::NUM_PROGRESS_STATES; ++idx )
	{
		StateProgressValues[ idx ] = 0.0f;
		StateProgressWeights[ idx ] = 1.0f;
	}

	// Update for initial values
	UpdateCachedValues();
	UpdateProgressInfo();
}

void FBuildPatchProgress::SetStateProgress( const EBuildPatchProgress::Type& State, const float& Value )
{
	check( !FMath::IsNaN( Value ) );
	FScopeLock ScopeLock( &ThreadLock );
	if( StateProgressValues[ State ] != Value )
	{
		StateProgressValues[ State ] = Value;
		UpdateProgressInfo();
	}
}

void FBuildPatchProgress::SetStateWeight( const EBuildPatchProgress::Type& State, const float& Value )
{
	check( !FMath::IsNaN( Value ) );
	FScopeLock ScopeLock( &ThreadLock );
	if( bCountsTowardsProgress[ State ] )
	{
		TotalWeight += Value - StateProgressWeights[ State ];
		StateProgressWeights[ State ] = Value;
	}
}

const FText& FBuildPatchProgress::GetStateText( bool ShortError )
{
	FScopeLock ScopeLock( &ThreadLock );
	if( CurrentState == EBuildPatchProgress::Error )
	{
		return ShortError ? ShortErrorText : ErrorText;
	}
	return EBuildPatchProgress::ToText( CurrentState );
}

const float FBuildPatchProgress::GetProgress()
{
	FScopeLock ScopeLock( &ThreadLock );
	if( bHasProgressValue[ CurrentState ] )
	{
		return CurrentProgress;
	}
	// We return -1.0f to indicate a marquee style progress should be displayed
	return -1.0f;
}

const float FBuildPatchProgress::GetStateProgress( const EBuildPatchProgress::Type& State )
{
	FScopeLock ScopeLock( &ThreadLock );
	return StateProgressValues[ State ];
}

const float FBuildPatchProgress::GetStateWeight(const EBuildPatchProgress::Type& State)
{
	FScopeLock ScopeLock( &ThreadLock );
	return StateProgressWeights[ State ];
}

const bool FBuildPatchProgress::TogglePauseState()
{
	FScopeLock ScopeLock( &ThreadLock );

	// If in pause state revert to no state and recalculate progress
	if( CurrentState == EBuildPatchProgress::Paused )
	{
		CurrentState = EBuildPatchProgress::NUM_PROGRESS_STATES;
		UpdateProgressInfo();
		return false;
	}
	else
	{
		// Else we just set paused
		CurrentState = EBuildPatchProgress::Paused;
		return true;
	}
}

const bool FBuildPatchProgress::GetPauseState()
{
	FScopeLock ScopeLock( &ThreadLock );
	return CurrentState == EBuildPatchProgress::Paused;
}

const double FBuildPatchProgress::WaitWhilePaused()
{
	// Pause if necessary
	const double PrePauseTime = FPlatformTime::Seconds();
	double PostPauseTime = PrePauseTime;
	while ( GetPauseState() )
	{
		FPlatformProcess::Sleep( 0.1f );
		PostPauseTime = FPlatformTime::Seconds();
	}
	// Return pause time
	return PostPauseTime - PrePauseTime;
}

void FBuildPatchProgress::UpdateProgressInfo()
{
	FScopeLock ScopeLock( &ThreadLock );

	// Override state to error if an error was set
	if( FBuildPatchInstallError::HasFatalError() )
	{
		CurrentState = EBuildPatchProgress::Error;
		ErrorText = FBuildPatchInstallError::GetErrorText();
		ShortErrorText = FBuildPatchInstallError::GetShortErrorText();
		return;
	}

	// If we are paused, we don't change the state or progress value
	if( CurrentState == EBuildPatchProgress::Paused )
	{
		return;
	}

	// Reset values
	CurrentState = EBuildPatchProgress::NUM_PROGRESS_STATES;
	CurrentProgress = 0.0f;

	// Loop through each progress value to find total progress and current state
	for( uint32 idx = 0; idx < EBuildPatchProgress::NUM_PROGRESS_STATES; ++idx )
	{
		// Is this the current state
		if( ( CurrentState == EBuildPatchProgress::NUM_PROGRESS_STATES ) && StateProgressValues[ idx ] < 1.0f )
		{
			CurrentState = static_cast< EBuildPatchProgress::Type >( idx );
		}

		// Do we count the progress
		if( bCountsTowardsProgress[ idx ] )
		{
			const float StateProgress = StateProgressValues[ idx ] * ( StateProgressWeights[ idx ] / TotalWeight );
			CurrentProgress += StateProgress;
			check( !FMath::IsNaN( CurrentProgress ) );
		}
	}

	// Ensure Sanity
	CurrentProgress = FMath::Clamp( CurrentProgress, 0.0f, 1.0f );
}

void FBuildPatchProgress::UpdateCachedValues()
{
	TotalWeight = 0.0f;
	for( uint32 idx = 0; idx < EBuildPatchProgress::NUM_PROGRESS_STATES; ++idx )
	{
		if( bCountsTowardsProgress[ idx ] )
		{
			TotalWeight += StateProgressWeights[ idx ];
		}
	}
	// If you assert here you're going to cause a divide by 0.0f
	check( TotalWeight != 0.0f );
}

/* FBuildPatchProgress static constants
*****************************************************************************/
const bool FBuildPatchProgress::bHasProgressValue[EBuildPatchProgress::NUM_PROGRESS_STATES] =
{
	false, // Initializing
	true,  // Resuming
	true,  // Downloading
	true,  // Installing
	true,  // MovingToInstall
	true,  // SettingAttributes
	true,  // BuildVerification
	false, // CleanUp
	false, // PrerequisitesInstall
	false, // Completed
	false, // Error
	true   // Paused
};

const bool FBuildPatchProgress::bCountsTowardsProgress[EBuildPatchProgress::NUM_PROGRESS_STATES] =
{
	false, // Initializing
	false, // Resuming
	true,  // Downloading
	true,  // Installing
	true,  // MovingToInstall
	true,  // SettingAttributes
	true,  // BuildVerification
	false, // CleanUp
	false, // PrerequisitesInstall
	false, // Completed
	false, // Error
	false  // Paused
};

#undef LOCTEXT_NAMESPACE
