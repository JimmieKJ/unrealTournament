// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchError.h: Declares classes involved setting and getting error information.
=============================================================================*/

#ifndef __BuildPatchError_h__
#define __BuildPatchError_h__

#pragma once

/**
 * Declares the error type enum for use with the fatal error system
 */
namespace EBuildPatchInstallError
{
	enum Type
	{
		// There has been no registered error
		NoError = 0,

		// A download registered a fatal error
		DownloadError = 1,

		// A file failed to construct properly
		FileConstructionFail = 2,

		// An error occurred trying to move the file to the install location
		MoveFileToInstall = 3,

		// The installed build failed to verify
		BuildVerifyFail = 4,

		// The user or some process has closed the application
		ApplicationClosing = 5,

		// An application error, such as module fail to load.
		ApplicationError = 6,

		// User canceled download
		UserCanceled = 7,

		// A prerequisites installer failed
		PrerequisiteError = 8,

		// An initialization error
		InitializationError = 9,

		// An error occurred creating a file due to excessive path length
		PathLengthExceeded = 10,

		// An error occurred creating a file due to their not being enough space left on the disk
		OutOfDiskSpace = 11
	};
};

/**
 * A struct to hold static fatal error flagging
 */
struct FBuildPatchInstallError
{

private:
	// Use thread safe access to the variables
	static FCriticalSection ThreadLock;

	// The current error state
	static EBuildPatchInstallError::Type ErrorState;

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
	static EBuildPatchInstallError::Type GetErrorState();

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
	static void SetFatalError( const EBuildPatchInstallError::Type& ErrorType, const FString& ErrorLog = TEXT( "" ) );

private:
	/**
	 * Returns the string representation of the specified FBuildPatchInstallError value. Used for logging only.
	 * @param ErrorType - The error type value.
	 * @return A string value.
	 */
	static const FString& ToString( const EBuildPatchInstallError::Type& ErrorType );

	/**
	 * Returns the string representation of the current FBuildPatchInstallError value. Used for logging only.
	 * @return A string value.
	 */
	static const FString& ToString();
};

#endif // __BuildPatchChunk_h__
