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
		|| ErrorState == EBuildPatchInstallError::InitializationError;
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

const FText& FBuildPatchInstallError::GetErrorText()
{
	static const FText NoError( LOCTEXT( "BuildPatchInstallError_NoError", "The operation was successful." ) );
	static const FText DownloadError( LOCTEXT( "BuildPatchInstallError_DownloadError", "Could not download patch data. Please try again later." ) );
	static const FText FileConstructionFail( LOCTEXT( "BuildPatchInstallError_FileConstructionFail", "A file corruption has occurred. Please try again." ) );
	static const FText MoveFileToInstall( LOCTEXT( "BuildPatchInstallError_MoveFileToInstall", "A file access error has occurred. Please check your running processes." ) );
	static const FText BuildVerifyFail( LOCTEXT( "BuildPatchInstallError_BuildCorrupt", "The installation is corrupt." ) );
	static const FText ApplicationClosing( LOCTEXT( "BuildPatchInstallError_ApplicationClosing", "The application is closing." ) );
	static const FText ApplicationError( LOCTEXT( "BuildPatchInstallError_ApplicationError", "Patching service could not start." ) );
	static const FText UserCanceled( LOCTEXT( "BuildPatchInstallError_UserCanceled", "User cancelled." ) );
	static const FText PrerequisiteError( LOCTEXT( "BuildPatchInstallError_PrerequisiteError", "Prerequisites install failed.") );
	static const FText InitializationError(LOCTEXT("BuildPatchInstallError_InitializationError", "The installer failed to initialize."));
	static const FText InvalidOrMax( LOCTEXT( "BuildPatchInstallError_InvalidOrMax", "An unknown error ocurred." ) );

	FScopeLock ScopeLock( &ThreadLock );
	switch( ErrorState )
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

