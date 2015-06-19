// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

DECLARE_DELEGATE_RetVal_OneParam(bool, FOnProcessMadeProgress, const FString&)

/**
 * Helper function to run custom command line.
 *
 * @param ExecutablePath Path to executable to run.
 * @param CommandLine Command line to run custom process.
 *
 * @returns True if succeeded. False otherwise.
 */
bool RunProcess(const FString& ExecutablePath, const FString& CommandLine = FString());

/**
 * Helper function to run custom command line and catch output.
 *
 * @param ExecutablePath Path to executable to run.
 * @param CommandLine Command line to run custom process.
 * @param Output Collected output.
 *
 * @returns True if succeeded. False otherwise.
 */
bool RunProcessOutput(const FString& ExecutablePath, const FString& CommandLine, FString& Output);

/**
 * Helper function to run custom command line and catch output.
 *
 * @param ExecutablePath Path to executable to run.
 * @param CommandLine Command line to run custom process.
 * @param OnUATMadeProgress Called when process make progress.
 *
 * @returns True if succeeded. False otherwise.
 */
bool RunProcessProgress(const FString& ExecutablePath, const FString& CommandLine, const FOnProcessMadeProgress& OnUATMadeProgress);

#if PLATFORM_WINDOWS
/**
 * Checks if process is currently running given full image path.
 *
 * @param FullImagePath Full path to the process executable.
 *
 * @returns True if it's running. False otherwise.
 */
bool IsRunningProcess(const FString& FullImagePath);
#endif