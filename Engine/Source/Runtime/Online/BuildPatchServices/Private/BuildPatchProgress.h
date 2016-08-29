// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchProgress.h: Declares classes involved with tracking the patch
	progress information.
=============================================================================*/

#pragma once

/**
 * Namespace to declares the progress type enum
 */
namespace EBuildPatchProgress
{
	enum Type
	{
		// The patch process is waiting for other installs
		Queued = 0,

		// The patch process is initializing
		Initializing,

		// The patch process is enumerating existing staged data
		Resuming,

		// The patch process is downloading patch data
		Downloading,

		// The patch process is installing files
		Installing,

		// The patch process is moving staged files to the install
		MovingToInstall,

		// The patch process is setting up attributes on the build
		SettingAttributes,

		// The patch process is verifying the build
		BuildVerification,

		// The patch process is cleaning temp files
		CleanUp,

		// The patch process is installing prerequisites
		PrerequisitesInstall,

		// A state to catch the UI when progress is 100% but UI still being displayed
		Completed,

		// The process has been set paused
		Paused,

		// Holds the number of states, for array sizes
		NUM_PROGRESS_STATES,
	};

	/**
	 * Returns the string representation of the EBuildPatchProgress value. Used for analytics and logging only.
	 * @param ProgressValue - The value.
	 * @return The enum's string value.
	 */
	const FString& ToString( const Type& ProgressValue );

	/**
	 * Returns the FText representation of the specified EBuildPatchProgress value. Used for displaying to the user.
	 * @param ProgressValue - The error type value.
	 * @return The display text associated with the progress step.
	 */
	const FText& ToText( const Type& ProgressValue );
}

/**
 * A struct to hold patch progress tracking
 */
struct FBuildPatchProgress
{
private:

	// Defines whether each state displays progress percent or is designed for a "please wait" or marquee style progress bar
	// This is predefined and constant.
	static const bool bHasProgressValue[ EBuildPatchProgress::NUM_PROGRESS_STATES ];

	// Defines whether each state should count towards the overall progress
	// This is predefined and constant.
	static const bool bCountsTowardsProgress[ EBuildPatchProgress::NUM_PROGRESS_STATES ];

	// Holds the current percentage complete for each state, this will decide the "current" state, being the first that is not complete.
	// Range 0 to 1.
	float StateProgressValues[ EBuildPatchProgress::NUM_PROGRESS_STATES ];

	// Holds the weight that each stage has on overall progress. 
	// Range 0 to 1.
	float StateProgressWeights[ EBuildPatchProgress::NUM_PROGRESS_STATES ];

	// Cached total weight value for progress calculation
	float TotalWeight;

	// The current state value for UI polling
	EBuildPatchProgress::Type CurrentState;

	// The current progress value for UI polling
	float CurrentProgress;

	// Critical section to protect variable access
	FCriticalSection ThreadLock;

public:

	/**
	 * Default constructor
	 */
	FBuildPatchProgress();

	/**
	 * Resets internal variables to start over
	 */
	void Reset();

	/**
	 * Sets the progress value for a particular state
	 * @param State		The state to set progress for
	 * @param Value		The progress value
	 */
	void SetStateProgress( const EBuildPatchProgress::Type& State, const float& Value );

	/**
	 * Sets the progress weight for a particular state
	 * @param State		The state to set weight for
	 * @param Value		The weight value
	 */
	void SetStateWeight( const EBuildPatchProgress::Type& State, const float& Value );

	/**
	 * Gets the text for the current progress state
	 * @return The display text for the current progress state
	 */
	const FText& GetStateText();

	/**
	 * Gets the current overall progress
	 * @return The current progress value. Range 0 to 1. -1 indicates undetermined, i.e. show a marquee style bar.
	 */
	float GetProgress();

	/**
	 * Gets the current overall progress regardless of current state using marquee
	 * @return The current progress value. Range 0 to 1.
	 */
	float GetProgressNoMarquee();

	/**
	 * Gets the progress value for a particular state
	 * @param State		The state to get progress for
	 * @return The state progress value. Range 0 to 1.
	 */
	float GetStateProgress( const EBuildPatchProgress::Type& State );

	/**
	 * Gets the weight value for a particular state
	 * @param State		The state to get weight for
	 * @return The state weight value.
	 */
	float GetStateWeight( const EBuildPatchProgress::Type& State );

	/**
	 * Toggles the pause state
	 * @return Whether the current state is now paused
	 */
	bool TogglePauseState();

	/**
	 * Blocks calling thread while the progress is paused
	 * @return How long we paused for, in seconds
	 */
	double WaitWhilePaused();

	/**
	 * Gets the pause state
	 * @return Whether the current state is paused
	 */
	bool GetPauseState();

private:

	/**
	 * Updates the current state and progress values
	 */
	void UpdateProgressInfo();

	/**
	 * Updates the cached values
	 */
	void UpdateCachedValues();
};
