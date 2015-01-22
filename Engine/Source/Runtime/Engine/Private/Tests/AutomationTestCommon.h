// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnginePrivate.h"


///////////////////////////////////////////////////////////////////////
// Common Latent commands which are used across test type. I.e. Engine, Network, etc...


/**
 * Force a matinee to not loop and request that it play
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FPlayMatineeLatentCommand, AMatineeActor*, MatineeActor);


/**
 * Wait for a particular matinee actor to finish playing
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FWaitForMatineeToCompleteLatentCommand, AMatineeActor*, MatineeActor);


/**
 * Execute command string
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FExecStringLatentCommand, FString, ExecCommand);


/**
 * Wait for the given amount of time
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FEngineWaitLatentCommand, float, Duration);


/**
 * Enqueue performance capture commands after a map has been loaded
 */
DEFINE_LATENT_AUTOMATION_COMMAND(FEnqueuePerformanceCaptureCommands);


/**
* Run FPS chart command using for the actual duration of the matinee.
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FMatineePerformanceCaptureCommand, FString, MatineeName);

/**
* Latent command to run an exec command that also requires a UWorld.
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FExecWorldStringLatentCommand, FString, ExecCommand);