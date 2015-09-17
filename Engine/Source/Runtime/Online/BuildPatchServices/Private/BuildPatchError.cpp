// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchError.cpp: Implements classes involved setting and getting error information.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"

#define LOCTEXT_NAMESPACE "BuildPatchInstallError"

/* FBuildPatchInstallError implementation
*****************************************************************************/
void FBuildPatchInstallError::Reset()
{
	FScopeLock ScopeLock( &ThreadLock );
	ErrorState = EBuildPatchInstallError::NoError;
}

bool FBuildPatchInstallError::HasFatalError()
{
	FScopeLock ScopeLock( &ThreadLock );
	return ErrorState != EBuildPatchInstallError::NoError;
}

EBuildPatchInstallError::Type FBuildPatchInstallError::GetErrorState()
{
	FScopeLock ScopeLock( &ThreadLock );
	return ErrorState;
}

bool FBuildPatchInstallError::IsInstallationCancelled()
{
	FScopeLock ScopeLock( &ThreadLock );
	return ErrorState == EBuildPatchInstallError::UserCanceled || ErrorState == EBuildPatchInstallError::ApplicationClosing;
}

bool FBuildPatchInstallError::IsNoRetryError()
{
	FScopeLock ScopeLock( &ThreadLock );
	return ErrorState == EBuildPatchInstallError::DownloadError
		|| ErrorState == EBuildPatchInstallError::MoveFileToInstall
		|| ErrorState == EBuildPatchInstallError::InitializationError
		|| ErrorState == EBuildPatchInstallError::PathLengthExceeded
		|| ErrorState == EBuildPatchInstallError::OutOfDiskSpace;
}

FString FBuildPatchInstallError::GetErrorString()
{
	FScopeLock ScopeLock( &ThreadLock );
	FString ReturnString = ToString();
	if ( HasFatalError() )
	{
		ReturnString += TEXT( " " );
		ReturnString += ErrorString;
	}
	return ReturnString;
}

const FText FBuildPatchInstallError::GetErrorText()
{
	static const FText DownloadError( LOCTEXT( "BuildPatchInstallError_DownloadError", "Please try again later." ) );
	static const FText FileConstructionFail( LOCTEXT( "BuildPatchInstallError_FileConstructionFail", "Please try again." ) );
	static const FText MoveFileToInstall( LOCTEXT( "BuildPatchInstallError_MoveFileToInstall", "Please check your running processes." ) );
	static const FText PathLengthExceeded(LOCTEXT("BuildPatchInstallError_PathLengthExceeded", "Please specify a shorter install location."));
	static const FText OutOfDiskSpace(LOCTEXT("BuildPatchInstallError_OutOfDiskSpace", "Please free up some disk space and try again."));

	FScopeLock ScopeLock( &ThreadLock );
	FText ErrorText = FText::GetEmpty();
	switch( ErrorState )
	{
		case EBuildPatchInstallError::DownloadError: ErrorText = DownloadError; break;
		case EBuildPatchInstallError::FileConstructionFail: ErrorText = FileConstructionFail; break;
		case EBuildPatchInstallError::MoveFileToInstall: ErrorText = MoveFileToInstall; break;
		case EBuildPatchInstallError::PathLengthExceeded: ErrorText = PathLengthExceeded; break;
		case EBuildPatchInstallError::OutOfDiskSpace: ErrorText = OutOfDiskSpace; break;
	}
	return FText::Format(LOCTEXT("BuildPatchInstallLongError", "{0} {1}"), GetShortErrorText(), ErrorText);
}

const FText& FBuildPatchInstallError::GetShortErrorText()
{
	static const FText NoError(LOCTEXT("BuildPatchInstallShortError_NoError", "The operation was successful."));
	static const FText DownloadError(LOCTEXT("BuildPatchInstallShortError_DownloadError", "Could not download patch data."));
	static const FText FileConstructionFail(LOCTEXT("BuildPatchInstallShortError_FileConstructionFail", "A file corruption has occurred."));
	static const FText MoveFileToInstall(LOCTEXT("BuildPatchInstallShortError_MoveFileToInstall", "A file access error has occurred."));
	static const FText BuildVerifyFail(LOCTEXT("BuildPatchInstallShortError_BuildCorrupt", "The installation is corrupt."));
	static const FText ApplicationClosing(LOCTEXT("BuildPatchInstallShortError_ApplicationClosing", "The application is closing."));
	static const FText ApplicationError(LOCTEXT("BuildPatchInstallShortError_ApplicationError", "Patching service could not start."));
	static const FText UserCanceled(LOCTEXT("BuildPatchInstallShortError_UserCanceled", "User cancelled."));
	static const FText PrerequisiteError(LOCTEXT("BuildPatchInstallShortError_PrerequisiteError", "Prerequisites install failed."));
	static const FText InitializationError(LOCTEXT("BuildPatchInstallShortError_InitializationError", "The installer failed to initialize."));
	static const FText PathLengthExceeded(LOCTEXT("BuildPatchInstallShortError_PathLengthExceeded", "Maximum path length exceeded."));
	static const FText OutOfDiskSpace(LOCTEXT("BuildPatchInstallShortError_OutOfDiskSpace", "Not enough disk space available."));
	static const FText InvalidOrMax(LOCTEXT("BuildPatchInstallShortError_InvalidOrMax", "An unknown error ocurred."));

	FScopeLock ScopeLock(&ThreadLock);
	switch (ErrorState)
	{
		case EBuildPatchInstallError::NoError: return NoError;
		case EBuildPatchInstallError::DownloadError: return DownloadError;
		case EBuildPatchInstallError::FileConstructionFail: return FileConstructionFail;
		case EBuildPatchInstallError::MoveFileToInstall: return MoveFileToInstall;
		case EBuildPatchInstallError::BuildVerifyFail: return BuildVerifyFail;
		case EBuildPatchInstallError::ApplicationClosing: return ApplicationClosing;
		case EBuildPatchInstallError::ApplicationError: return ApplicationError;
		case EBuildPatchInstallError::UserCanceled: return UserCanceled;
		case EBuildPatchInstallError::PrerequisiteError: return PrerequisiteError;
		case EBuildPatchInstallError::InitializationError: return InitializationError;
		case EBuildPatchInstallError::PathLengthExceeded: return PathLengthExceeded;
		case EBuildPatchInstallError::OutOfDiskSpace: return OutOfDiskSpace;
		default: return InvalidOrMax;
	}
}

void FBuildPatchInstallError::SetFatalError( const EBuildPatchInstallError::Type& ErrorType, const FString& ErrorLog )
{
	FScopeLock ScopeLock( &ThreadLock );
	// Only accept the first error
	if( HasFatalError() )
	{
		return;
	}
	if( ErrorType != EBuildPatchInstallError::NoError)
	{
		// Log first as the fatal error
		GWarn->Logf( TEXT( "BuildDataGenerator: FATAL ERROR: %s, %s" ), *ToString( ErrorType ), *ErrorLog );
	}
	else
	{
		// Log any that follow, but they are likely caused by the first
		GWarn->Logf( TEXT( "BuildDataGenerator: Secondary error: %s, %s" ), *ToString( ErrorType ), *ErrorLog );
	}
	ErrorState = ErrorType;
	ErrorString = ErrorLog;
}

const FString& FBuildPatchInstallError::ToString( const EBuildPatchInstallError::Type& ErrorType )
{
	// Const enum strings, special case no error.
	static const FString NoError( "SUCCESS" );
	static const FString DownloadError( "EBuildPatchInstallError::DownloadError" );
	static const FString FileConstructionFail( "EBuildPatchInstallError::FileConstructionFail" );
	static const FString MoveFileToInstall( "EBuildPatchInstallError::MoveFileToInstall" );
	static const FString BuildVerifyFail( "EBuildPatchInstallError::BuildVerifyFail" );
	static const FString ApplicationClosing( "EBuildPatchInstallError::ApplicationClosing" );
	static const FString ApplicationError( "EBuildPatchInstallError::ApplicationError" );
	static const FString UserCanceled( "EBuildPatchInstallError::UserCanceled" );
	static const FString PrerequisiteError( "EBuildPatchInstallError::PrerequisiteError" );
	static const FString InitializationError("EBuildPatchInstallError::InitializationError");
	static const FString PathLengthExceeded("EBuildPatchInstallError::PathLengthExceeded");
	static const FString OutOfDiskSpace("EBuildPatchInstallError::OutOfDiskSpace");
	static const FString InvalidOrMax( "EBuildPatchInstallError::InvalidOrMax" );

	FScopeLock ScopeLock( &ThreadLock );
	switch( ErrorType )
	{
		case EBuildPatchInstallError::NoError: return NoError;
		case EBuildPatchInstallError::DownloadError: return DownloadError;
		case EBuildPatchInstallError::FileConstructionFail: return FileConstructionFail;
		case EBuildPatchInstallError::MoveFileToInstall: return MoveFileToInstall;
		case EBuildPatchInstallError::BuildVerifyFail: return BuildVerifyFail;
		case EBuildPatchInstallError::ApplicationClosing: return ApplicationClosing;
		case EBuildPatchInstallError::ApplicationError: return ApplicationError;
		case EBuildPatchInstallError::UserCanceled: return UserCanceled;
		case EBuildPatchInstallError::PrerequisiteError: return PrerequisiteError;
		case EBuildPatchInstallError::InitializationError: return InitializationError;
		case EBuildPatchInstallError::PathLengthExceeded: return PathLengthExceeded;
		case EBuildPatchInstallError::OutOfDiskSpace: return OutOfDiskSpace;
		default: return InvalidOrMax;
	}
}

const FString& FBuildPatchInstallError::ToString()
{
	return ToString( GetErrorState() );
}

/**
 * Static FBuildPatchInstallError variables
 */
FCriticalSection FBuildPatchInstallError::ThreadLock;
EBuildPatchInstallError::Type FBuildPatchInstallError::ErrorState = EBuildPatchInstallError::NoError;
FString FBuildPatchInstallError::ErrorString;

#undef LOCTEXT_NAMESPACE

