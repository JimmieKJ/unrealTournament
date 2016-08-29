// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchError.h: Declares classes involved setting and getting error information.
=============================================================================*/

#ifndef __BuildPatchError_h__
#define __BuildPatchError_h__

#pragma once

// Protect against namespace collision.
namespace BuildPatchConstants
{
	/**
	 * A list of the error codes we will use for each case of initialization failure.
	 */
	namespace InitializationErrorCodes
	{
		static const TCHAR* MissingStageDirectory = TEXT("01");
		static const TCHAR* MissingInstallDirectory = TEXT("02");
	}

	/**
	 * A list of the error codes we will use for each case of running out of disk space.
	 */
	namespace DiskSpaceErrorCodes
	{
		static const TCHAR* InitialSpaceCheck = TEXT("01");
		static const TCHAR* DuringInstallation = TEXT("02");
	}

	/**
	 * A list of the error codes we will use for each case of exceeding path length.
	 */
	namespace PathLengthErrorCodes
	{
		static const TCHAR* StagingDirectory = TEXT("01");
	}

	/**
	 * A list of the error codes we will use for each case of download failure.
	 */
	namespace DownloadErrorCodes
	{
		static const TCHAR* OutOfRetries = TEXT("01");
	}

	/**
	 * A list of the error codes we will use for each case of file construction failure.
	 */
	namespace ConstructionErrorCodes
	{
		static const TCHAR* UnknownFail = TEXT("01");
		static const TCHAR* FileCreateFail = TEXT("02");
		static const TCHAR* MissingChunkData = TEXT("03");
		static const TCHAR* MissingFileInfo = TEXT("04");
		static const TCHAR* OutboundCorrupt = TEXT("05");
	}

	/**
	 * A list of the error codes we will use for each case of moving files.
	 */
	namespace MoveErrorCodes
	{
		static const TCHAR* StageToInstall = TEXT("01");
	}

	/**
	 * A list of the error codes we will use for each case of verification failure.
	 */
	namespace VerifyErrorCodes
	{
		static const TCHAR* FinalCheck = TEXT("01");
	}

	/**
	 * A list of the error codes we will use for each case of cancellation.
	 */
	namespace UserCancelErrorCodes
	{
		static const TCHAR* UserRequested = TEXT("01");
	}

	/**
	 * A list of the error codes we will use for each case of Application Closing.
	 */
	namespace ApplicationClosedErrorCodes
	{
		static const TCHAR* ApplicationClosed = TEXT("01");
	}

	/**
	 * A list of the error codes we will use for each case of cancellation.
	 */
	namespace PrerequisiteErrorPrefixes
	{
		static const TCHAR* ExecuteCode = TEXT("E");
		static const TCHAR* ReturnCode = TEXT("R");
	}
}

/**
 * A struct to hold static fatal error flagging.
 */
struct FBuildPatchInstallError
{

private:
	// Use thread safe access to the variables.
	static FCriticalSection ThreadLock;

	// The current error type.
	static EBuildPatchInstallError ErrorType;

	// The current error code for the error type.
	static FString ErrorCode;

public:
	/**
	 * Static function to reset the state.
	 */
	static void Reset();

	/**
	 * Static function to get if there has been a fatal error.
	 * @return True if a fatal error has been set.
	 */
	static bool HasFatalError();

	/**
	 * Static function to get if error state indicates installation should cancel.
	 * @return True if the installation should cancel without retry.
	 */
	static bool IsInstallationCancelled();

	/**
	 * Static function to get if error state indicates a network related error. It's not helpful to keep retrying.
	 * the installation stage if this is the case.
	 * @return True if the installation should cancel without retry.
	 */
	static bool IsNoRetryError();

	/**
	 * Static function to get fatal error.
	 * @return The fatal error type.
	 */
	static EBuildPatchInstallError GetErrorState();

	/**
	 * Static function to get error code for the failure.
	 * @return The error code.
	 */
	static FString GetErrorCode();

	/**
	 * Static function to get a localized error text for UI. These are for example, and it is recommended to look up your own based on GetErrorState() or GetErrorCode().
	 * @return The error text.
	 */
	static const FText& GetErrorText();

	/**
	 * Static function allowing any worker to set fatal error.
	 * @param ErrorType     The error type value.
	 * @param ErrorCode     String with specific error code for this error type.
	 */
	static void SetFatalError(const EBuildPatchInstallError& ErrorType, const FString& ErrorCode);

	/**
	 * Returns the string representation of the specified FBuildPatchInstallError value. Used for logging only.
	 * @param ErrorType     The error type value.
	 * @return A string value for the EBuildPatchInstallError enum.
	 */
	static const FString& EnumToString(const EBuildPatchInstallError& ErrorType);
};

#endif // __BuildPatchChunk_h__
