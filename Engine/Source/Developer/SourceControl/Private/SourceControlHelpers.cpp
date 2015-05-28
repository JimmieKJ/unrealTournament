// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SourceControlPrivatePCH.h"
#include "SourceControlHelpers.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "ISourceControlModule.h"
#include "ISourceControlLabel.h"
#include "ISourceControlRevision.h"
#include "CoreUObject.h"
#include "MessageLog.h"

#define LOCTEXT_NAMESPACE "SourceControlHelpers"

namespace SourceControlHelpers
{

const FString& GetSettingsIni()
{
	if(ISourceControlModule::Get().GetUseGlobalSettings())
	{
		return GetGlobalSettingsIni();
	}
	else
	{
		static FString SourceControlSettingsIni;
		if(SourceControlSettingsIni.Len() == 0)
		{
			const FString SourceControlSettingsDir = FPaths::GeneratedConfigDir();
			FConfigCacheIni::LoadGlobalIniFile(SourceControlSettingsIni, TEXT("SourceControlSettings"), NULL, false, false, true, *SourceControlSettingsDir);
		}
		return SourceControlSettingsIni;
	}
}

const FString& GetGlobalSettingsIni()
{
	static FString SourceControlGlobalSettingsIni;
	if(SourceControlGlobalSettingsIni.Len() == 0)
	{
		const FString SourceControlSettingsDir = FPaths::EngineSavedDir() + TEXT("Config/");
		FConfigCacheIni::LoadGlobalIniFile(SourceControlGlobalSettingsIni, TEXT("SourceControlSettings"), NULL, false, false, true, *SourceControlSettingsDir);
	}
	return SourceControlGlobalSettingsIni;
}

static FString PackageFilename_Internal( const FString& InPackageName )
{
	FString Filename = InPackageName;

	// Get the filename by finding it on disk first
	if ( !FPackageName::DoesPackageExist(InPackageName, NULL, &Filename) )
	{
		// The package does not exist on disk, see if we can find it in memory and predict the file extension
		// Only do this if the supplied package name is valid
		const bool bIncludeReadOnlyRoots = false;
		if ( FPackageName::IsValidLongPackageName(InPackageName, bIncludeReadOnlyRoots) )
		{
			UPackage* Package = FindPackage(NULL, *InPackageName);
			if ( Package )
			{
				// This is a package in memory that has not yet been saved. Determine the extension and convert to a filename
				const FString PackageExtension = Package->ContainsMap() ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
				Filename = FPackageName::LongPackageNameToFilename(InPackageName, PackageExtension);
			}
		}
	}

	return Filename;
}

FString PackageFilename( const FString& InPackageName )
{
	return FPaths::ConvertRelativePathToFull(PackageFilename_Internal(InPackageName));
}

FString PackageFilename( const UPackage* InPackage )
{
	FString Filename;
	if(InPackage != NULL)
	{
		Filename = FPaths::ConvertRelativePathToFull(PackageFilename_Internal(InPackage->GetName()));
	}
	return Filename;
}

TArray<FString> PackageFilenames( const TArray<UPackage*>& InPackages )
{
	TArray<FString> OutNames;
	for (int32 PackageIndex = 0; PackageIndex < InPackages.Num(); PackageIndex++)
	{
		OutNames.Add(FPaths::ConvertRelativePathToFull(PackageFilename(InPackages[PackageIndex])));
	}

	return OutNames;
}

TArray<FString> PackageFilenames( const TArray<FString>& InPackageNames )
{
	TArray<FString> OutNames;
	for (int32 PackageIndex = 0; PackageIndex < InPackageNames.Num(); PackageIndex++)
	{
		OutNames.Add(FPaths::ConvertRelativePathToFull(PackageFilename_Internal(InPackageNames[PackageIndex])));
	}

	return OutNames;
}

TArray<FString> AbsoluteFilenames( const TArray<FString>& InFileNames )
{
	TArray<FString> AbsoluteFiles;
	for(const auto& FileName : InFileNames)
	{
		if(!FPaths::IsRelative(FileName))
		{
			AbsoluteFiles.Add(FileName);
		}
		else
		{
			AbsoluteFiles.Add(FPaths::ConvertRelativePathToFull(FileName));
		}
	}

	return AbsoluteFiles;
}

void RevertUnchangedFiles( ISourceControlProvider& InProvider, const TArray<FString>& InFiles )
{
	// Make sure we update the modified state of the files
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateStatusOperation->SetUpdateModifiedState(true);
	InProvider.Execute(UpdateStatusOperation, InFiles);

	TArray<FString> UnchangedFiles;
	TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> > OutStates;
	InProvider.GetState(InFiles, OutStates, EStateCacheUsage::Use);

	for(TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >::TConstIterator It(OutStates); It; It++)
	{
		TSharedRef<ISourceControlState, ESPMode::ThreadSafe> SourceControlState = *It;
		if(SourceControlState->IsCheckedOut() && !SourceControlState->IsModified())
		{
			UnchangedFiles.Add(SourceControlState->GetFilename());
		}
	}

	if(UnchangedFiles.Num())
	{
		InProvider.Execute( ISourceControlOperation::Create<FRevert>(), UnchangedFiles );
	}
}

bool AnnotateFile( ISourceControlProvider& InProvider, const FString& InLabel, const FString& InFile, TArray<FAnnotationLine>& OutLines )
{
	TArray< TSharedRef<ISourceControlLabel> > Labels = InProvider.GetLabels( InLabel );
	if(Labels.Num() > 0)
	{
		TSharedRef<ISourceControlLabel> Label = Labels[0];
		TArray< TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> > Revisions;
		Label->GetFileRevisions(InFile, Revisions);
		if(Revisions.Num() > 0)
		{
			TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> Revision = Revisions[0];
			if(Revision->GetAnnotated(OutLines))
			{
				return true;
			}
		}
	}

	return false;
}

bool AnnotateFile( ISourceControlProvider& InProvider, int32 InCheckInIdentifier, const FString& InFile, TArray<FAnnotationLine>& OutLines )
{
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateStatusOperation->SetUpdateHistory(true);
	if(InProvider.Execute(UpdateStatusOperation, InFile) == ECommandResult::Succeeded)
	{
		FSourceControlStatePtr State = InProvider.GetState(InFile, EStateCacheUsage::Use);
		if(State.IsValid())
		{
			for(int32 HistoryIndex = State->GetHistorySize() - 1; HistoryIndex >= 0; HistoryIndex--)
			{
				// check that the changelist corresponds to this revision - we assume history is in latest-first order
				TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision = State->GetHistoryItem(HistoryIndex);
				if(Revision.IsValid() && Revision->GetCheckInIdentifier() >= InCheckInIdentifier)
				{
					if(Revision->GetAnnotated(OutLines))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool CheckOutFile( const FString& InFilePath )
{
	if ( InFilePath.IsEmpty() )
	{
		FMessageLog("SourceControl").Error(LOCTEXT("UnspecifiedCheckoutFile", "Check out file not specified"));
		return false;
	}

	if( !ISourceControlModule::Get().IsEnabled() )
	{
		FMessageLog("SourceControl").Error(LOCTEXT("SourceControlDisabled", "Source control is not enabled."));
		return false;
	}

	if( !ISourceControlModule::Get().GetProvider().IsAvailable() )
	{
		FMessageLog("SourceControl").Error(LOCTEXT("SourceControlServerUnavailable", "Source control server is currently not available."));
		return false;
	}

	bool bSuccessfullyCheckedOut = false;
	TArray<FString> FilesToBeCheckedOut;
	FilesToBeCheckedOut.Add( InFilePath );

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState( InFilePath, EStateCacheUsage::ForceUpdate );
	if(SourceControlState.IsValid())
	{
		FString SimultaneousCheckoutUser;
		if( SourceControlState->IsAdded() ||
			SourceControlState->IsCheckedOut())
		{
			// Already checked out or opened for add
			bSuccessfullyCheckedOut = true;
		}
		else
		{
			if(SourceControlState->CanCheckout())
			{
				bSuccessfullyCheckedOut = (SourceControlProvider.Execute( ISourceControlOperation::Create<FCheckOut>(), FilesToBeCheckedOut ) == ECommandResult::Succeeded);
				if (!bSuccessfullyCheckedOut)
				{
					FFormatNamedArguments Arguments;
					Arguments.Add( TEXT("InFilePath"), FText::FromString(InFilePath) );
					FMessageLog("SourceControl").Error(FText::Format(LOCTEXT("CheckoutFailed", "Failed to check out file '{InFilePath}'."), Arguments));
				}
			}
			else if(!SourceControlState->IsSourceControlled())
			{
				bSuccessfullyCheckedOut = (SourceControlProvider.Execute( ISourceControlOperation::Create<FMarkForAdd>(), FilesToBeCheckedOut ) == ECommandResult::Succeeded);
				if (!bSuccessfullyCheckedOut)
				{
					FFormatNamedArguments Arguments;
					Arguments.Add( TEXT("InFilePath"), FText::FromString(InFilePath) );
					FMessageLog("SourceControl").Error(FText::Format(LOCTEXT("AddFailed", "Failed to add file '{InFilePath}' to source control."), Arguments));
				}
			}
			else if(!SourceControlState->IsCurrent())
			{
				FFormatNamedArguments Arguments;
				Arguments.Add( TEXT("InFilePath"), FText::FromString(InFilePath) );
				FMessageLog("SourceControl").Error(FText::Format(LOCTEXT("NotAtHeadRevision", "File '{InFilePath}' is not at head revision."), Arguments));
			}
			else if(SourceControlState->IsCheckedOutOther(&(SimultaneousCheckoutUser)))
			{
				FFormatNamedArguments Arguments;
				Arguments.Add( TEXT("InFilePath"), FText::FromString(InFilePath) );
				Arguments.Add( TEXT("SimultaneousCheckoutUser"), FText::FromString(SimultaneousCheckoutUser) );
				FMessageLog("SourceControl").Error(FText::Format(LOCTEXT("SimultaneousCheckout", "File '{InFilePath}' is checked out by another ('{SimultaneousCheckoutUser}')."), Arguments));
			}
			else
			{
				// Improper or invalid SCC state
				FFormatNamedArguments Arguments;
				Arguments.Add( TEXT("InFilePath"), FText::FromString(InFilePath) );
				FMessageLog("SourceControl").Error(FText::Format(LOCTEXT("CouldNotDetermineState", "Could not determine source control state of file '{InFilePath}'."), Arguments));
			}
		}
	}
	else
	{
		// Improper or invalid SCC state
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("InFilePath"), FText::FromString(InFilePath) );
		FMessageLog("SourceControl").Error(FText::Format(LOCTEXT("CouldNotDetermineState", "Could not determine source control state of file '{InFilePath}'."), Arguments));
	}

	return bSuccessfullyCheckedOut;
}

bool CheckoutOrMarkForAdd( const FString& InDestFile, const FText& InFileDescription, const FOnPostCheckOut& OnPostCheckOut, FText& OutFailReason )
{
	bool bSucceeded = true;

	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();

	// first check for source control check out
	if (ISourceControlModule::Get().IsEnabled())
	{
		FSourceControlStatePtr SourceControlState = Provider.GetState(InDestFile, EStateCacheUsage::ForceUpdate);
		if (SourceControlState.IsValid())
		{
			if (SourceControlState->IsSourceControlled() && SourceControlState->CanCheckout())
				{
					ECommandResult::Type Result = Provider.Execute(ISourceControlOperation::Create<FCheckOut>(), InDestFile);
					bSucceeded = (Result == ECommandResult::Succeeded);
					if (!bSucceeded)
					{
						OutFailReason = FText::Format(LOCTEXT("SourceControlCheckoutError", "Could not check out {0} file."), InFileDescription);
					}
				}
		}
	}

	if (bSucceeded)
	{
		if(OnPostCheckOut.IsBound())
		{
			bSucceeded = OnPostCheckOut.Execute(InDestFile, InFileDescription, OutFailReason);
		}
	}

	// mark for add now if needed
	if (bSucceeded && ISourceControlModule::Get().IsEnabled())
	{
		FSourceControlStatePtr SourceControlState = Provider.GetState(InDestFile, EStateCacheUsage::Use);
		if (SourceControlState.IsValid())
		{
			if (!SourceControlState->IsSourceControlled())
			{
				ECommandResult::Type Result = Provider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), InDestFile);
				bSucceeded = (Result == ECommandResult::Succeeded);
				if (!bSucceeded)
				{
					OutFailReason = FText::Format(LOCTEXT("SourceControlMarkForAddError", "Could not mark {0} file for add."), InFileDescription);
				}
			}
		}
	}

	return bSucceeded;
}

bool CopyFileUnderSourceControl( const FString& InDestFile, const FString& InSourceFile, const FText& InFileDescription, FText& OutFailReason)
{
	struct Local
	{
		static bool CopyFile(const FString& InDestinationFile, const FText& InFileDesc, FText& OutFailureReason, FString InFileToCopy)
		{
			const bool bReplace = true;
			const bool bEvenIfReadOnly = true;
			bool bSucceeded = (IFileManager::Get().Copy(*InDestinationFile, *InFileToCopy, bReplace, bEvenIfReadOnly) == COPY_OK);
			if (!bSucceeded)
			{
				OutFailureReason = FText::Format(LOCTEXT("ExternalImageCopyError", "Could not overwrite {0} file."), InFileDesc);
			}

			return bSucceeded;
		}
	};

	return CheckoutOrMarkForAdd(InDestFile, InFileDescription, FOnPostCheckOut::CreateStatic(&Local::CopyFile, InSourceFile), OutFailReason);
}

bool BranchPackage( UPackage* DestPackage, UPackage* SourcePackage )
{
	if(ISourceControlModule::Get().IsEnabled())
	{
		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

		const FString SourceFilename = PackageFilename(SourcePackage);
		const FString DestFilename = PackageFilename(DestPackage);
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceFilename, EStateCacheUsage::ForceUpdate);
		if(SourceControlState.IsValid() && SourceControlState->IsSourceControlled())
		{
			TSharedRef<FCopy, ESPMode::ThreadSafe> CopyOperation = ISourceControlOperation::Create<FCopy>();
			CopyOperation->SetDestination(DestFilename);
			
			return (SourceControlProvider.Execute(CopyOperation, SourceFilename) == ECommandResult::Succeeded);
		}
	}

	return false;
}

}

FScopedSourceControl::FScopedSourceControl()
{
	ISourceControlModule::Get().GetProvider().Init();
}

FScopedSourceControl::~FScopedSourceControl()
{
	ISourceControlModule::Get().GetProvider().Close();
}

ISourceControlProvider& FScopedSourceControl::GetProvider()
{
	return ISourceControlModule::Get().GetProvider();
}

#undef LOCTEXT_NAMESPACE