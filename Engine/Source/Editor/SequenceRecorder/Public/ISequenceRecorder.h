// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"

DECLARE_DELEGATE_OneParam(FOnRecordingStarted, class UMovieSceneSequence* /*Sequence*/);

DECLARE_DELEGATE_OneParam(FOnRecordingFinished, class UMovieSceneSequence* /*Sequence*/);

class ISequenceRecorder : public IModuleInterface
{
public:
	/** 
	 * Start recording the passed-in actors.
	 * @param	World			The world we use to record actors
	 * @param	ActorFilter		Actor filter to gather actors spawned in this world for recording
	 * @return true if recording was successfully started
	 */
	virtual bool StartRecording(UWorld* World, const struct FSequenceRecorderActorFilter& ActorFilter) = 0;

	/** Stop recording current sequence, if any */
	virtual void StopRecording() = 0;

	/** Are we currently recording a sequence */
	virtual bool IsRecording() = 0;

	/** How long is the currently recording sequence */
	virtual float GetCurrentRecordingLength() = 0;

	/**
	 * Start a recording, possibly with some delay (specified by the sequence recording settings).
	 * @param	World				The world we use to record actors
	 * @param	ActorNameToRecord	The name of the actor to record (fuzzy-matched, precise name not required).
	 * @param	OnRecordingStarted	Delegate fired when recording has commenced.
	 * @param	OnRecordingFinished	Delegate fired when recording has finished.
	 * @param	PathToRecordTo		Optional path to a sequence to record to. If none is specified we use the defaults in the settings.
	 * @param	SequenceName		Optional name of a sequence to record to. If none is specified we use the defaults in the settings.
	 * @return true if recording was successfully started
	*/
	virtual bool StartRecording(UWorld* World, const FString& ActorNameToRecord, const FOnRecordingStarted& OnRecordingStarted, const FOnRecordingFinished& OnRecordingFinished, const FString& PathToRecordTo = FString(), const FString& SequenceName = FString()) = 0;

	/**
	 * Notify we should start recording an actor - usually used for 'actor pooling' implementations
	 * to simulate spawning. Has no effect when recording is not in progress.
	 * @param	Actor	The actor that was 'spawned'
	 */
	virtual void NotifyActorStartRecording(AActor* Actor) = 0;

	/**
	 * Notify we should stop recording an actor - usually used for 'actor pooling' implementations
	 * to simulate de-spawning. Has no effect when recording is not in progress.
	 * @param	Actor	The actor that was 'de-spawned'
	 */
	virtual void NotifyActorStopRecording(AActor* Actor) = 0;
};