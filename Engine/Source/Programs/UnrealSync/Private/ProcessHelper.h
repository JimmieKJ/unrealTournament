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
bool RunProcess(const FString& ExecutablePath, const FString& CommandLine);

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