// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchError.h: Declares classes involved setting and getting error information.
=============================================================================*/

#ifndef __BuildPatchError_h__
#define __BuildPatchError_h__

#pragma once

/**
 * A struct to hold static fatal error flagging
 */
struct FBuildPatchInstallError
{

private:
	// Use thread safe access to the variables
	static FCriticalSection ThreadLock;

	// The current error state
	static EBuildPatchInstallError ErrorState;

	// The current error string
	static FString ErrorString;

public:
	/**
	 * Static function to reset the state
	 */
	static void Reset();

	/**
	 * Static function to get if there has been a fatal error
	 * @return True if a fatal error has been set
	 */
	static bool HasFatalError();

	/**
	 * Static function to get if error state indicates installation should cancel
	 * @return True if the installation should cancel without retry
	 */
	static bool IsInstallationCancelled();

	/**
	 * Static function to get if error state indicates a network related error. It's not helpful to keep retrying
	 * the installation stage if this is the case.
	 * @return True if the installation should cancel without retry
	 */
	static bool IsNoRetryError();

	/**
	 * Static function to get fatal error
	 * @return The fatal error type
	 */
	static EBuildPatchInstallError GetErrorState();

	/**
	 * Static function to get the error string for logging
	 * @return The error string
	 */
	static FString GetErrorString();

	/**
	 * Static function to get the error text for UI
	 * @return The error text
	 */
	static const FText GetErrorText();

	/**
	 * Static function to get the truncated error text for UI
	 * @return The shorter error text
	 */
	static const FText& GetShortErrorText();

	/**
	 * Static function allowing any worker to set fatal error
	 * @param ErrorType		The error type value.
	 * @param ErrorLog		An optional string with extra information.
	 */
	static void SetFatalError( const EBuildPatchInstallError& ErrorType, const FString& ErrorLog = TEXT( "" ) );

private:
	/**
	 * Returns the string representation of the specified FBuildPatchInstallError value. Used for logging only.
	 * @param ErrorType - The error type value.
	 * @return A string value.
	 */
	static const FString& ToString( const EBuildPatchInstallError& ErrorType );

	/**
	 * Returns the string representation of the current FBuildPatchInstallError value. Used for logging only.
	 * @return A string value.
	 */
	static const FString& ToString();
};

#endif // __BuildPatchChunk_h__
