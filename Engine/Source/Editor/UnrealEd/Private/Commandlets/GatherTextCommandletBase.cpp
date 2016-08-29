// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Internationalization/InternationalizationMetadata.h"
#include "Json.h"
#include "JsonInternationalizationManifestSerializer.h"
#include "JsonInternationalizationMetadataSerializer.h"
#include "ISourceControlModule.h"
#include "AssetRegistryModule.h"
#include "PackageHelperFunctions.h"
#include "ObjectTools.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextCommandletBase, Log, All);

//////////////////////////////////////////////////////////////////////////
//UGatherTextCommandletBase

UGatherTextCommandletBase::UGatherTextCommandletBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGatherTextCommandletBase::Initialize( const TSharedPtr< FLocTextHelper >& InGatherManifestHelper, const TSharedPtr< FGatherTextSCC >& InSourceControlInfo )
{
	GatherManifestHelper = InGatherManifestHelper;
	SourceControlInfo = InSourceControlInfo;
}

void UGatherTextCommandletBase::CreateCustomEngine(const FString& Params)
{
	GEngine = GEditor = NULL;//Force a basic default engine. 
}

bool UGatherTextCommandletBase::GetBoolFromConfig( const TCHAR* Section, const TCHAR* Key, bool& OutValue, const FString& Filename )
{
	bool bSuccess = GConfig->GetBool( Section, Key, OutValue, Filename );
	
	if( !bSuccess )
	{
		bSuccess = GConfig->GetBool( TEXT("CommonSettings"), Key, OutValue, Filename );
	}
	return bSuccess;
}

bool UGatherTextCommandletBase::GetStringFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename )
{
	bool bSuccess = GConfig->GetString( Section, Key, OutValue, Filename );

	if( !bSuccess )
	{
		bSuccess = GConfig->GetString( TEXT("CommonSettings"), Key, OutValue, Filename );
	}
	return bSuccess;
}

bool UGatherTextCommandletBase::GetPathFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename )
{
	bool bSuccess = GetStringFromConfig( Section, Key, OutValue, Filename );

	if( bSuccess )
	{
		if (FPaths::IsRelative(OutValue))
		{
			if (!FPaths::GameDir().IsEmpty())
			{
				OutValue = FPaths::Combine( *( FPaths::GameDir() ), *OutValue );
			}
			else
			{
				OutValue = FPaths::Combine( *( FPaths::EngineDir() ), *OutValue );
			}
		}
	}
	return bSuccess;
}

int32 UGatherTextCommandletBase::GetStringArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename )
{
	int32 count = GConfig->GetArray( Section, Key, OutArr, Filename );

	if( count == 0 )
	{
		count = GConfig->GetArray( TEXT("CommonSettings"), Key, OutArr, Filename );
	}
	return count;
}

int32 UGatherTextCommandletBase::GetPathArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename )
{
	int32 count = GetStringArrayFromConfig( Section, Key, OutArr, Filename );

	for (int32 i = 0; i < count; ++i)
	{
		if (FPaths::IsRelative(OutArr[i]))
		{
			const FString ProjectBasePath = FPaths::GameDir().IsEmpty() ? FPaths::EngineDir() : FPaths::GameDir();
			OutArr[i] = FPaths::Combine( *ProjectBasePath, *OutArr[i] );
			OutArr[i] = FPaths::ConvertRelativePathToFull(OutArr[i]);
		}
		FPaths::CollapseRelativeDirectories(OutArr[i]);
	}
	return count;
}


const TCHAR* FWordCountReportData::EntryDelimiter = TEXT(",");
const TCHAR* FWordCountReportData::NewLineDelimiter = TEXT("\n");
const FString FWordCountReportData::ColHeadingDateTime = TEXT("Date/Time");
const FString FWordCountReportData::ColHeadingWordCount = TEXT("Word Count");

bool FWordCountReportData::HasHeader() const
{
	// No real good way to decide if there is a header so here we just check to see if the first character is a letter
	if( Data.Num() > 0 && Data[0].Num() > 0 && Data[0][0].Len() > 0 )
	{
		return FChar::IsAlpha( Data[0][0][0] );
	}
	return false;
}

int32 FWordCountReportData::GetColumnCount() const
{
	if( Data.Num() > 0 )
	{
		return Data[0].Num();
	}
	return 0;
}


int32 FWordCountReportData::GetRowCount() const
{
	if( Data.Num() > 0 )
	{
		return Data.Num();
	}
	return 0;
}

const FString* FWordCountReportData::GetColumnHeader(int32 ColumnIndex) const
{
	if( ColumnIndex >= 0 && HasHeader() && ColumnIndex < GetColumnCount() )
	{
		return &Data[0][ColumnIndex];
	}
	return NULL;
}

const FString* FWordCountReportData::GetEntry(int32 RowIndex, int32 ColumnIndex) const
{
	if( RowIndex >= 0 && ColumnIndex >= 0 && RowIndex < GetRowCount() && ColumnIndex < GetColumnCount() )
	{
		return &Data[RowIndex][ColumnIndex];
	}
	return NULL;
}

bool FWordCountReportData::FromCSV( const FString& InString )
{
	Data.Empty();

	if( InString.Len() == 0 )
	{
		// An empty input string is valid but there is nothing to process
		return true;
	}

	FString CSVString = InString.Replace(TEXT("\r\n"), NewLineDelimiter);
	// Note, we do a trivial parse here and assume every delimiter we see separates entries/rows and is not contained within an entry
	TArray<FString> CSVRows;

	int32 NumRows = CSVString.ParseIntoArray( CSVRows, NewLineDelimiter, true );

	for(int32 RowIdx = 0; RowIdx < NumRows; RowIdx++ )
	{
		TArray< FString > RowEntriesArray;
		CSVRows[RowIdx].ParseIntoArray( RowEntriesArray, EntryDelimiter, false);
		Data.Add(RowEntriesArray);
	}

	// Do some validation of the data
	NumRows = GetRowCount();
	int32 NumCols = GetColumnCount();
	for(int32 RowIdx = 1; RowIdx < NumRows; RowIdx++ )
	{
		// Make sure each row has the same number of column entries
		if( !ensureMsgf(NumCols == Data[RowIdx].Num(), TEXT("Row index %d has an invalid number of columns"), RowIdx) )
		{
			return false;
		}
	}

	return true;
}

FString FWordCountReportData::ToCSV() const
{
	FString OutputStr;
	for(int32 RowIdx = 0; RowIdx < Data.Num(); RowIdx++)
	{
		const TArray< FString >& Row = Data[RowIdx];
		for(int32 RowEntry = 0; RowEntry < Row.Num(); RowEntry++)
		{
			OutputStr += Row[RowEntry];
			if(RowEntry < Row.Num() - 1)
			{
				OutputStr += EntryDelimiter;
			}
		}
		if(RowIdx < Data.Num() - 1)
		{
			OutputStr += NewLineDelimiter;
		}
	}
	return OutputStr;
}

int32 FWordCountReportData::GetColumnIndex( const FString& ColumnHeading ) const
{
	if(HasHeader())
	{
		const TArray< FString >& Row = Data[0];
		return Row.Find(ColumnHeading);
	}
	return INDEX_NONE;
}

void FWordCountReportData::SetEntry( int32 RowIndex, int32 ColumnIndex, const FString& EntryString )
{
	if( RowIndex >= 0 && ColumnIndex >= 0 && RowIndex < GetRowCount() && ColumnIndex < GetColumnCount() )
	{
		Data[RowIndex][ColumnIndex] = EntryString;
	}
}

void FWordCountReportData::SetEntry( int32 RowIndex, const FString& ColumnHeading, const FString& EntryString )
{
	int32 ColumnIndex = GetColumnIndex(ColumnHeading);
	if(INDEX_NONE != ColumnIndex)
	{
		SetEntry(RowIndex, ColumnIndex, EntryString);
	}
	else
	{
		ColumnIndex = AddColumn(&ColumnHeading);
		SetEntry(RowIndex, ColumnIndex, EntryString);
	}

}

int32 FWordCountReportData::AddRow()
{
	TArray< FString > RowEntriesArray;
	for(int32 ColIndex = 0; ColIndex < GetColumnCount(); ColIndex++)
	{
		RowEntriesArray.Add(FString(TEXT("")));
	}
	return Data.Add(RowEntriesArray);
}

int32 FWordCountReportData::AddColumn(const FString* ColumnHeading )
{
	int32 RowIndex = 0;
	if( ColumnHeading != NULL && HasHeader() )
	{
		// Check to see if a column with the provided header already exists
		int32 ExistingColumnIndex = GetColumnIndex(*ColumnHeading);
		if(ExistingColumnIndex != INDEX_NONE)
		{
			return ExistingColumnIndex;
		}

		Data[0].Add(*ColumnHeading);
		RowIndex++;
	}

	for(;RowIndex < Data.Num(); RowIndex++)
	{
		Data[RowIndex].Add(FString(TEXT("")));
	}

	return GetColumnCount() - 1;
}


FGatherTextSCC::FGatherTextSCC()
{
	ISourceControlModule::Get().GetProvider().Init();
}

FGatherTextSCC::~FGatherTextSCC()
{
	if( CheckedOutFiles.Num() > 0 )
	{
		UE_LOG(LogGatherTextCommandletBase, Log, TEXT("Source Control wrapper shutting down with checked out files."));
	}

	ISourceControlModule::Get().GetProvider().Close();
}

bool FGatherTextSCC::CheckOutFile(const FString& InFile, FText& OutError)
{
	if ( InFile.IsEmpty() )
	{
		OutError = NSLOCTEXT("GatherTextCmdlet", "InvalidFileSpecified", "Could not checkout file at invalid path.");
		return false;
	}

	if ( InFile.StartsWith(TEXT("\\\\")) )
	{
		// We can't check out a UNC path, but don't say we failed
		return true;
	}

	FText SCCError;
	if( !IsReady( SCCError ) )
	{
		OutError = SCCError;
		return false;
	}

	FString AbsoluteFilename = FPaths::ConvertRelativePathToFull( InFile );

	if( CheckedOutFiles.Contains( AbsoluteFilename ) )
	{
		return true;
	}

	bool bSuccessfullyCheckedOut = false;
	TArray<FString> FilesToBeCheckedOut;
	FilesToBeCheckedOut.Add( AbsoluteFilename );

	FFormatNamedArguments Args;
	Args.Add(TEXT("Filepath"), FText::FromString(InFile));

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState( AbsoluteFilename, EStateCacheUsage::ForceUpdate );
	
	if(SourceControlState.IsValid() && SourceControlState->IsDeleted())
	{
		// If it's deleted, we need to revert that first
		SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), FilesToBeCheckedOut);
		SourceControlState = SourceControlProvider.GetState(AbsoluteFilename, EStateCacheUsage::ForceUpdate);
	}
	
	if(SourceControlState.IsValid())
	{
		FString Other;
		if( SourceControlState->IsAdded() ||
			SourceControlState->IsCheckedOut())
		{
			// Already checked out or opened for add
			bSuccessfullyCheckedOut = true;
		}
		else if(SourceControlState->CanCheckout())
		{
			bSuccessfullyCheckedOut = (SourceControlProvider.Execute( ISourceControlOperation::Create<FCheckOut>(), FilesToBeCheckedOut ) == ECommandResult::Succeeded);
			if (!bSuccessfullyCheckedOut)
			{
				OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "FailedToCheckOutFile", "Failed to check out file '{Filepath}'."), Args);
			}
		}
		else if(!SourceControlState->IsSourceControlled() && SourceControlState->CanAdd())
		{
			bSuccessfullyCheckedOut = (SourceControlProvider.Execute( ISourceControlOperation::Create<FMarkForAdd>(), FilesToBeCheckedOut ) == ECommandResult::Succeeded);
			if (!bSuccessfullyCheckedOut)
			{
				OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "FailedToAddFileToSourceControl", "Failed to add file '{Filepath}' to source control."), Args);
			}
		}
		else if(!SourceControlState->IsCurrent())
		{
			OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "FileIsNotAtHeadRevision", "File '{Filepath}' is not at head revision."), Args);
		}
		else if(SourceControlState->IsCheckedOutOther(&(Other)))
		{
			Args.Add(TEXT("Username"), FText::FromString(Other));
			OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "FileIsAlreadyCheckedOutByAnotherUser", "File '{Filepath}' is checked out by another ('{Username}')."), Args);
		}
		else
		{
			// Improper or invalid SCC state
			OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "CouldNotGetStateOfFile", "Could not determine source control state of file '{Filepath}'."), Args);
		}
	}
	else
	{
		// Improper or invalid SCC state
		OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "CouldNotGetStateOfFile", "Could not determine source control state of file '{Filepath}'."), Args);
	}

	if( bSuccessfullyCheckedOut )
	{
		CheckedOutFiles.AddUnique(AbsoluteFilename);
	}

	return bSuccessfullyCheckedOut;
}

bool FGatherTextSCC::CheckinFiles( const FText& InChangeDescription, FText& OutError )
{
	if( CheckedOutFiles.Num() == 0 )
	{
		return true;
	}

	FText SCCError;
	if( !IsReady( SCCError ) )
	{
		OutError = SCCError;
		return false;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	SourceControlHelpers::RevertUnchangedFiles(SourceControlProvider, CheckedOutFiles);

	// remove unchanged files
	for (int32 VerifyIndex = CheckedOutFiles.Num()-1; VerifyIndex >= 0; --VerifyIndex)
	{
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState( CheckedOutFiles[VerifyIndex], EStateCacheUsage::ForceUpdate );
		if (SourceControlState.IsValid() && !SourceControlState->IsCheckedOut() && !SourceControlState->IsAdded())
		{
			CheckedOutFiles.RemoveAt(VerifyIndex);
		}
	}

	if (CheckedOutFiles.Num() > 0)
	{
		TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
		CheckInOperation->SetDescription(InChangeDescription);
		if (!SourceControlProvider.Execute( CheckInOperation, CheckedOutFiles ))
		{
			OutError = NSLOCTEXT("GatherTextCmdlet", "FailedToCheckInFiles", "The checked out localization files could not be checked in.");
			return false;
		}
		CheckedOutFiles.Empty();
	}
	return true;
}

bool FGatherTextSCC::CleanUp(FText& OutError)
{
	if( CheckedOutFiles.Num() == 0 )
	{
		return true;
	}

	bool bCleanupSuccess = true;
	FString AccumulatedErrorsStr;
	const int32 FileCount = CheckedOutFiles.Num();
	int32 FileIdx = 0;
	for( int32 i = 0; i < FileCount; ++i )
	{
		FText ErrorText;
		bool bReverted = RevertFile(CheckedOutFiles[FileIdx], ErrorText);
		bCleanupSuccess &= bReverted;

		if( !bReverted )
		{
			if( !AccumulatedErrorsStr.IsEmpty() )
			{
				AccumulatedErrorsStr += TEXT(", ");
			}
			AccumulatedErrorsStr += FString::Printf(TEXT("%s : %s"), *CheckedOutFiles[FileIdx], *ErrorText.ToString());
			++FileIdx;
		}
	}

	if( !bCleanupSuccess )
	{
		OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "CouldNotCompleteSourceControlCleanup", "Could not complete Source Control cleanup.  {FailureReason}"), FText::FromString(AccumulatedErrorsStr));
	}

	return bCleanupSuccess;
}

bool FGatherTextSCC::IsReady(FText& OutError)
{
	if( !ISourceControlModule::Get().IsEnabled() )
	{
		OutError = NSLOCTEXT("GatherTextCmdlet", "SourceControlNotEnabled", "Source control is not enabled.");
		return false;
	}

	if( !ISourceControlModule::Get().GetProvider().IsAvailable() )
	{
		OutError = NSLOCTEXT("GatherTextCmdlet", "SourceControlNotAvailable", "Source control server is currently not available.");
		return false;
	}
	return true;
}

bool FGatherTextSCC::RevertFile( const FString& InFile, FText& OutError )
{
	if( InFile.IsEmpty() || InFile.StartsWith(TEXT("\\\\")) )
	{
		OutError = NSLOCTEXT("GatherTextCmdlet", "CouldNotRevertFile", "Could not revert file.");
		return false;
	}

	FText SCCError;
	if( !IsReady( SCCError ) )
	{
		OutError = SCCError;
		return false;
	}

	FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(InFile);
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState( AbsoluteFilename, EStateCacheUsage::ForceUpdate );

	bool bSuccessfullyReverted = false;

	TArray<FString> FilesToRevert;
	FilesToRevert.Add(AbsoluteFilename);

	if( SourceControlState.IsValid() && !SourceControlState->IsCheckedOut() && !SourceControlState->IsAdded() )
	{
		bSuccessfullyReverted = true;
	}
	else
	{
		bSuccessfullyReverted = (SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), FilesToRevert) == ECommandResult::Succeeded);
	}
	
	if(!bSuccessfullyReverted)
	{
		OutError = NSLOCTEXT("GatherTextCmdlet", "CouldNotRevertFile", "Could not revert file.");
	}
	else
	{
		CheckedOutFiles.Remove( AbsoluteFilename );
	}
	return bSuccessfullyReverted;
}

void FLocFileSCCNotifies::PreFileWrite(const FString& InFilename)
{
	if (SourceControlInfo.IsValid() && FPaths::FileExists(InFilename))
	{
		// File already exists, so check it out before writing to it
		FText ErrorMsg;
		if (!SourceControlInfo->CheckOutFile(InFilename, ErrorMsg))
		{
			UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Failed to check out file '%s'. %s"), *InFilename, *ErrorMsg.ToString());
		}
	}
}

void FLocFileSCCNotifies::PostFileWrite(const FString& InFilename)
{
	if (SourceControlInfo.IsValid())
	{
		// If the file didn't exist before then this will add it, otherwise it will do nothing
		FText ErrorMsg;
		if (!SourceControlInfo->CheckOutFile(InFilename, ErrorMsg))
		{
			UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Failed to check out file '%s'. %s"), *InFilename, *ErrorMsg.ToString());
		}
	}
}

bool FLocalizedAssetSCCUtil::SaveAssetWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, UObject* InAsset)
{
	UPackage* const AssetPackage = InAsset->GetOutermost();

	if (!AssetPackage)
	{
		UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Unable to find package for %s '%s'."), *InAsset->GetClass()->GetName(), *InAsset->GetPathName());
		return false;
	}

	return SavePackageWithSCC(InSourceControlInfo, AssetPackage);
}

bool FLocalizedAssetSCCUtil::SaveAssetWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, UObject* InAsset, const FString& InFilename)
{
	UPackage* const AssetPackage = InAsset->GetOutermost();

	if (!AssetPackage)
	{
		UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Unable to find package for %s '%s'."), *InAsset->GetClass()->GetName(), *InAsset->GetPathName());
		return false;
	}

	return SavePackageWithSCC(InSourceControlInfo, AssetPackage, InFilename);
}

bool FLocalizedAssetSCCUtil::SavePackageWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, UPackage* InPackage)
{
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(InPackage->GetPathName(), FPackageName::GetAssetPackageExtension());
	return SavePackageWithSCC(InSourceControlInfo, InPackage, PackageFileName);
}

bool FLocalizedAssetSCCUtil::SavePackageWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, UPackage* InPackage, const FString& InFilename)
{
	const bool bPackageExistedOnDisk = FPaths::FileExists(InFilename);

	// If the package already exists on disk, then we need to check it out before writing to it
	if (bPackageExistedOnDisk && InSourceControlInfo.IsValid())
	{
		FText SCCErrorStr;
		if (!InSourceControlInfo->CheckOutFile(InFilename, SCCErrorStr))
		{
			UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Failed to check-out package '%s' at '%s'. %s"), *InPackage->GetPathName(), *InFilename, *SCCErrorStr.ToString());
		}
	}

	if (!SavePackageHelper(InPackage, InFilename))
	{
		UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Unable to save updated package '%s' to '%s'"), *InPackage->GetPathName(), *InFilename);
		return false;
	}

	// If the package didn't exist on disk, then we need to check it out after writing to it (which will perform an add)
	if (!bPackageExistedOnDisk && InSourceControlInfo.IsValid())
	{
		FText SCCErrorStr;
		if (!InSourceControlInfo->CheckOutFile(InFilename, SCCErrorStr))
		{
			UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Failed to check-out package '%s' at '%s'. %s"), *InPackage->GetPathName(), *InFilename, *SCCErrorStr.ToString());
		}
	}

	return true;
}

bool FLocalizedAssetSCCUtil::DeleteAssetWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, UObject* InAsset)
{
	UPackage* const AssetPackage = InAsset->GetOutermost();

	if (!AssetPackage)
	{
		UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Unable to find package for %s '%s'."), *InAsset->GetClass()->GetName(), *InAsset->GetPathName());
		return false;
	}

	return DeletePackageWithSCC(InSourceControlInfo, AssetPackage);
}

bool FLocalizedAssetSCCUtil::DeletePackageWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, UPackage* InPackage)
{
	if (!ObjectTools::DeleteSingleObject(InPackage, /*bPerformReferenceCheck*/false))
	{
		return false;
	}

	TArray<UPackage*> DeletedPackages;
	DeletedPackages.Add(InPackage);
	ObjectTools::CleanupAfterSuccessfulDelete(DeletedPackages, /*bPerformReferenceCheck*/false);

	return true;
}

bool FLocalizedAssetSCCUtil::SaveFileWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, const FString& InFilename, const FSaveFileCallback& InSaveFileCallback)
{
	const bool bFileExistedOnDisk = FPaths::FileExists(InFilename);

	// If the file already exists on disk, then we need to check it out before writing to it
	if (bFileExistedOnDisk && InSourceControlInfo.IsValid())
	{
		FText SCCErrorStr;
		if (!InSourceControlInfo->CheckOutFile(InFilename, SCCErrorStr))
		{
			UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Failed to check-out file at '%s'. %s"), *InFilename, *SCCErrorStr.ToString());
		}
	}

	if (!InSaveFileCallback(InFilename))
	{
		UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Unable to save updated file to '%s'"), *InFilename);
		return false;
	}

	// If the file didn't exist on disk, then we need to check it out after writing to it (which will perform an add)
	if (!bFileExistedOnDisk && InSourceControlInfo.IsValid())
	{
		FText SCCErrorStr;
		if (!InSourceControlInfo->CheckOutFile(InFilename, SCCErrorStr))
		{
			UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Failed to check-out file at '%s'. %s"), *InFilename, *SCCErrorStr.ToString());
		}
	}

	return true;
}

bool FLocalizedAssetUtil::GetAssetsByPathAndClass(IAssetRegistry& InAssetRegistry, const FName InPackagePath, const FName InClassName, const bool bIncludeLocalizedAssets, TArray<FAssetData>& OutAssets)
{
	TArray<FName> PackagePaths;
	PackagePaths.Add(InPackagePath);

	return GetAssetsByPathAndClass(InAssetRegistry, PackagePaths, InClassName, bIncludeLocalizedAssets, OutAssets);
}

bool FLocalizedAssetUtil::GetAssetsByPathAndClass(IAssetRegistry& InAssetRegistry, const TArray<FName>& InPackagePaths, const FName InClassName, const bool bIncludeLocalizedAssets, TArray<FAssetData>& OutAssets)
{
	FARFilter AssetFilter;
	AssetFilter.PackagePaths = InPackagePaths;
	AssetFilter.bRecursivePaths = true;
	AssetFilter.ClassNames.Add(InClassName);
	AssetFilter.bRecursiveClasses = true;

	if (!InAssetRegistry.GetAssets(AssetFilter, OutAssets))
	{
		return false;
	}

	if (!bIncludeLocalizedAssets)
	{
		// We need to manually exclude any localized assets
		OutAssets.RemoveAll([&](const FAssetData& InAssetData) -> bool
		{
			return FPackageName::IsLocalizedPackage(InAssetData.PackageName.ToString());
		});
	}

	return true;
}


FFuzzyPathMatcher::FFuzzyPathMatcher(const TArray<FString>& InIncludePathFilters, const TArray<FString>& InExcludePathFilters)
{
	FuzzyPaths.Reserve(InIncludePathFilters.Num() + InExcludePathFilters.Num());

	for (const FString& IncludePath : InIncludePathFilters)
	{
		FuzzyPaths.Add(FFuzzyPath(IncludePath, EPathType::Include));
	}

	for (const FString& ExcludePath : InExcludePathFilters)
	{
		FuzzyPaths.Add(FFuzzyPath(ExcludePath, EPathType::Exclude));
	}

	// Sort the paths so that deeper paths with fewer wildcards appear first in the list
	FuzzyPaths.Sort([](const FFuzzyPath& PathOne, const FFuzzyPath& PathTwo) -> bool
	{
		auto GetFuzzRating = [](const FFuzzyPath& InFuzzyPath) -> int32
		{
			int32 PathDepth = 0;
			int32 PathFuzz = 0;
			for (const TCHAR Char : InFuzzyPath.PathFilter)
			{
				if (Char == TEXT('/') || Char == TEXT('\\'))
				{
					++PathDepth;
				}
				else if (Char == TEXT('*') || Char == TEXT('?'))
				{
					++PathFuzz;
				}
			}

			return (100 - PathDepth) + (PathFuzz * 1000);
		};

		const int32 PathOneFuzzRating = GetFuzzRating(PathOne);
		const int32 PathTwoFuzzRating = GetFuzzRating(PathTwo);
		return PathOneFuzzRating < PathTwoFuzzRating;
	});
}

FFuzzyPathMatcher::EPathMatch FFuzzyPathMatcher::TestPath(const FString& InPathToTest) const
{
	for (const FFuzzyPath& FuzzyPath : FuzzyPaths)
	{
		if (InPathToTest.MatchesWildcard(FuzzyPath.PathFilter))
		{
			return (FuzzyPath.PathType == EPathType::Include) ? EPathMatch::Included : EPathMatch::Excluded;
		}
	}

	return EPathMatch::NoMatch;
}
