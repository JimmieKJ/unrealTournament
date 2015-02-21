// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FixupRedirectsCommandlet.cpp: Object redirect cleaner commandlet
=============================================================================*/

#include "UnrealEd.h"
#include "ISourceControlModule.h"
#include "Engine/UserDefinedEnum.h"
#include "AutoSaveUtils.h"
#include "AssetRegistryModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogFixupRedirectsCommandlet, Log, All);

UFixupRedirectsCommandlet::UFixupRedirectsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LogToConsole = false;
}


void UFixupRedirectsCommandlet::CreateCustomEngine(const FString& Params)
{
	// by making sure these are NULL, the caller will create the default engine object
	GEngine = GEditor = NULL;
}

int32 UFixupRedirectsCommandlet::Main( const FString& Params )
{
	// Loading asset registry during serialization below will assert
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList;
	FEditorFileUtils::FindAllPackageFiles(PackageList);
	if( !PackageList.Num() )
	{
		return 0;
	}

	// process the commandline
	FString Token;
	const TCHAR* CommandLine	= *Params;
	bool bIsQuietMode			= false;
	bool bIsTestOnly			= false;
	bool bShouldRestoreProgress= false;
	bool bIsSCCDisabled		= false;
	bool bUpdateEnginePackages = false;
	bool bDeleteRedirects		= true;
	bool bDeleteOnlyReferencedRedirects = false;
	bool bAutoSubmit			= false;
	bool bSandboxed			= IFileManager::Get().IsSandboxEnabled();

	while (FParse::Token(CommandLine, Token, 1))
	{
		if (Token == TEXT("-nowarn"))
		{
			bIsQuietMode = true;
		}
		else if (Token == TEXT("-testonly"))
		{
			bIsTestOnly = true;
		}
		else if (Token == TEXT("-restore"))
		{
			bShouldRestoreProgress = true;
		}
		else if (Token == TEXT("-NoAutoCheckout"))
		{
			bIsSCCDisabled = true;
		}
		else if (Token == TEXT("-AutoSubmit"))
		{
			bAutoSubmit = true;
		}
		else if (Token == TEXT("-UpdateEnginePackages"))
		{
			bUpdateEnginePackages = true;
		}
		else if (Token == TEXT("-NoDelete"))
		{
			bDeleteRedirects = false;
		}
		else if (Token == TEXT("-NoDeleteUnreferenced"))
		{
			bDeleteOnlyReferencedRedirects = true;
		}
		else if ( Token.Left(1) != TEXT("-") )
		{
			FPackageName::SearchForPackageOnDisk(Token, &GRedirectCollector.FileToFixup);
		}
	}

	if (bSandboxed)
	{
		UE_LOG(LogFixupRedirectsCommandlet, Display, TEXT("Running in a sandbox."));
		bIsSCCDisabled = true;
		bAutoSubmit = false;
		bDeleteRedirects = false;
	}


	bool bShouldSkipErrors = false;

	// Setup file Output log in the Saved Directory
	const FString ProjectDir = FPaths::GetPath(FPaths::GetProjectFilePath());
	const FString LogFilename = FPaths::Combine(*ProjectDir, TEXT("Saved"), TEXT("FixupRedirect"), TEXT("FixupRedirect.log"));
	FArchive* LogOutputFile = IFileManager::Get().CreateFileWriter(*LogFilename);
	LogOutputFile->Logf(TEXT("[Redirects]"));

	/////////////////////////////////////////////////////////////////////
	// Display any script code referenced redirects
	/////////////////////////////////////////////////////////////////////

	TArray<FString> UpdatePackages;
	TArray<FString> RedirectorsThatCantBeCleaned;

	if (!bShouldRestoreProgress)
	{
		// load all string asset reference targets, and add fake redirectors for them
		GRedirectCollector.ResolveStringAssetReference();

		for (int32 RedirIndex = 0; RedirIndex < GRedirectCollector.Redirections.Num(); RedirIndex++)
		{
			FRedirection& Redir = GRedirectCollector.Redirections[RedirIndex];
			UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Boot redirector can't be cleaned: %s(%s) -> %s(%s)"), *Redir.RedirectorName, *Redir.RedirectorPackageFilename, *Redir.DestinationObjectName, *Redir.PackageFilename);
			RedirectorsThatCantBeCleaned.AddUnique(Redir.RedirectorName);
		}

		/////////////////////////////////////////////////////////////////////
		// Build a list of packages that need to be updated
		/////////////////////////////////////////////////////////////////////

		SET_WARN_COLOR(COLOR_WHITE);
		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT(""));
		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Generating list of tasks:"));
		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("-------------------------"));
		CLEAR_WARN_COLOR();

		int32 GCIndex = 0;

		// process script code first pass, then everything else
		for( int32 PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
		{
			const FString& Filename = PackageList[PackageIndex];

			// already loaded code, skip them, and skip autosave packages
			if (Filename.StartsWith(AutoSaveUtils::GetAutoSaveDir(), ESearchCase::IgnoreCase))
			{
				continue;
			}

			UE_LOG(LogFixupRedirectsCommandlet, Display, TEXT("Looking for redirects in %s..."), *Filename);

			// Assert if package couldn't be opened so we have no chance of messing up saving later packages.
			UPackage* Package = Cast<UPackage>(LoadPackage(NULL, *Filename, 0));

			// load all string asset reference targets, and add fake redirectors for them
			GRedirectCollector.ResolveStringAssetReference();

			if (!Package)
			{
				SET_WARN_COLOR(COLOR_RED);
				UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("   ... failed to open %s"), *Filename);
				CLEAR_WARN_COLOR();

				// if we are not in quiet mode, and the user wants to continue, go on
				const FText Message = FText::Format( NSLOCTEXT("Unreal", "PackageFailedToOpen", "The package '{0}' failed to open. Should this commandlet continue operation, ignoring further load errors? Continuing may have adverse effects (object references may be lost)."), FText::FromString( Filename ) );
				if (bShouldSkipErrors || (!bIsQuietMode && GWarn->YesNof( Message )))
				{
					bShouldSkipErrors = true;
					continue;
				}
				else
				{
					return 1;
				}
			}

			// all notices here about redirects get this color
			SET_WARN_COLOR(COLOR_DARK_GREEN);

			// look for any redirects that were followed that are referenced by this package
			// (may have been loaded earlier if this package was loaded by script code)
			// any redirectors referenced by script code can't be cleaned up
			for (int32 RedirIndex = 0; RedirIndex < GRedirectCollector.Redirections.Num(); RedirIndex++)
			{
				FRedirection& Redir = GRedirectCollector.Redirections[RedirIndex];

				if (Redir.PackageFilename == Filename)
				{
					UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("   ... references %s [-> %s]"), *Redir.RedirectorName, *Redir.DestinationObjectName);

					// put the source and dest packages in the list to be updated
					UpdatePackages.AddUnique(Redir.PackageFilename);
					UpdatePackages.AddUnique(Redir.RedirectorPackageFilename);
				}
			}

			// clear the flag for needing to collect garbage
			bool bNeedToCollectGarbage = false;

			// if this package has any redirectors, make sure we update it
			if (!GRedirectCollector.FileToFixup.Len() || GRedirectCollector.FileToFixup == FPackageName::FilenameToLongPackageName(Filename))
			{
				TArray<UObject *> ObjectsInOuter;
				GetObjectsWithOuter(Package, ObjectsInOuter);
				for( int32 Index = 0; Index < ObjectsInOuter.Num(); Index++ )
				{
					UObject* Obj = ObjectsInOuter[Index];
					UObjectRedirector* Redir = Cast<UObjectRedirector>(Obj);
					if (Redir)
					{
						// make sure this package is in the list of packages to update
						UpdatePackages.AddUnique(Filename);

						UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("   ... has redirect %s [-> %s]"), *Redir->GetPathName(), *Redir->DestinationObject->GetFullName());
						LogOutputFile->Logf(TEXT("%s = %s"), *Redir->GetPathName(), *Redir->DestinationObject->GetFullName());

						// make sure we GC if we found a redirector
						bNeedToCollectGarbage = true;
					}
				}
			}

			CLEAR_WARN_COLOR();

			// collect garbage every N packages, or if there was any redirectors in the package 
			// (to make sure that redirectors get reloaded properly and followed by the callback)
			// also, GC after loading a map package, to make sure it's unloaded so that we can track
			// other packages loading a map package as a dependency
			if (((++GCIndex) % 50) == 0 || bNeedToCollectGarbage || Package->ContainsMap())
			{
				// collect garbage to close the package
				CollectGarbage(RF_Native);
				UE_LOG(LogFixupRedirectsCommandlet, Display, TEXT("GC..."));
				// reset our counter
				GCIndex = 0;
			}
		}

		// save off restore point
		FArchive* Ar = IFileManager::Get().CreateFileWriter(*(FPaths::GameSavedDir() + TEXT("Fixup.bin")));
		*Ar << GRedirectCollector.Redirections;
		*Ar << UpdatePackages;
		*Ar << RedirectorsThatCantBeCleaned;
		delete Ar;
	}
	else
	{
		// load restore point
		FArchive* Ar = IFileManager::Get().CreateFileReader(*(FPaths::GameSavedDir() + TEXT("Fixup.bin")));
		if( Ar != NULL )
		{
			*Ar << GRedirectCollector.Redirections;
			*Ar << UpdatePackages;
			*Ar << RedirectorsThatCantBeCleaned;
			delete Ar;
		}
	}

	// unregister the callback so we stop getting redirections added
	FCoreUObjectDelegates::RedirectorFollowed.RemoveAll(&GRedirectCollector);
	
	/////////////////////////////////////////////////////////////////////
	// Explain to user what is happening
	/////////////////////////////////////////////////////////////////////
	SET_WARN_COLOR(COLOR_YELLOW);
	UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT(""));
	UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Files that need to fixed up:"));
	SET_WARN_COLOR(COLOR_DARK_YELLOW);

	// print out the working set of packages
	for (int32 PackageIndex = 0; PackageIndex < UpdatePackages.Num(); PackageIndex++)
	{
		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("   %s"), *UpdatePackages[PackageIndex]);
	}

	UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT(""));
	CLEAR_WARN_COLOR();

	// if we are only testing, just just quiet before actually doing anything
	if (bIsTestOnly)
	{
		LogOutputFile->Close();
		delete LogOutputFile;
		LogOutputFile = NULL;
		return 0;
	}

	/////////////////////////////////////////////////////////////////////
	// Find redirectors that are referenced by packages we cant check out
	/////////////////////////////////////////////////////////////////////

	bool bEngineRedirectorCantBeCleaned = false;

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	// initialize source control if it hasn't been initialized yet
	if(!bIsSCCDisabled)
	{
		SourceControlProvider.Init();

		//Make sure we can check out all packages - update their state first
		SourceControlProvider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), UpdatePackages);
	}

	for( int32 PackageIndex = 0; PackageIndex < UpdatePackages.Num(); PackageIndex++ )
	{
		const FString& Filename = UpdatePackages[PackageIndex];

		bool bCanEdit = true;

		if(!bIsSCCDisabled)
		{
			FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(Filename, EStateCacheUsage::ForceUpdate);
			if(SourceControlState.IsValid() && !SourceControlState->CanEdit())
			{
				bCanEdit = false;
			}
		}
		else if(IFileManager::Get().IsReadOnly(*Filename))
		{
			bCanEdit = false;
		}

		if(!bCanEdit)
		{
			const bool bAllowCheckout = bUpdateEnginePackages || !Filename.StartsWith( FPaths::EngineDir() );

			if (!bIsSCCDisabled && bAllowCheckout)
			{
				FString PackageName(FPackageName::FilenameToLongPackageName(Filename));
				FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(Filename, EStateCacheUsage::ForceUpdate);
				if (SourceControlState.IsValid() && SourceControlState->CanCheckout())
				{
					SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), SourceControlHelpers::PackageFilename(PackageName));
					FSourceControlStatePtr NewSourceControlState = SourceControlProvider.GetState(Filename, EStateCacheUsage::ForceUpdate);
					bCanEdit = NewSourceControlState.IsValid() && NewSourceControlState->CanEdit();
				}
			}

			// if the checkout failed for any reason, we can't clean it up
			if (bAllowCheckout && !bCanEdit)
			{
				SET_WARN_COLOR(COLOR_RED);
				UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Couldn't check out/edit %s..."), *Filename);
				CLEAR_WARN_COLOR();

				// load all string asset reference targets, and add fake redirectors for them
				GRedirectCollector.ResolveStringAssetReference();

				// any redirectors that are pointed to by this read-only package can't be fixed up
				for (int32 RedirIndex = 0; RedirIndex < GRedirectCollector.Redirections.Num(); RedirIndex++)
				{
					FRedirection& Redir = GRedirectCollector.Redirections[RedirIndex];

					// any redirectors pointed to by this file we don't clean
					if (Redir.PackageFilename == Filename)
					{
						RedirectorsThatCantBeCleaned.AddUnique(Redir.RedirectorName);
						UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("   ... can't fixup references to %s"), *Redir.RedirectorName);

						// if this redirector is in an Engine package, we need to alert the user, because
						// Engine packages are shared between games, so if one game still needs the redirector
						// to be valid, then we can't check in Engine packages after another game has
						// run that could 
						if (!bEngineRedirectorCantBeCleaned)
						{
							FString PackagePath;
							FPackageName::DoesPackageExist(Redir.RedirectorPackageFilename, NULL, &PackagePath);
							if (PackagePath.StartsWith(FPaths::EngineDir()))
							{
								bEngineRedirectorCantBeCleaned = true;
							}
						}
					}
					// any redirectors in this file can't be deleted
					else if (Redir.RedirectorPackageFilename == Filename)
					{
						RedirectorsThatCantBeCleaned.AddUnique(Redir.RedirectorName);
						UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("   ... can't delete %s"), *Redir.RedirectorName);
					}
				}
			} 
			else
			{
				SET_WARN_COLOR(COLOR_DARK_GREEN);
				UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Checked out %s..."), *Filename);
				CLEAR_WARN_COLOR();
			}
		}
	}
	// warn about an engine package that can't be cleaned up (may want to revert engine packages)
	if (bEngineRedirectorCantBeCleaned)
	{
		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT(""));
		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Engine package redirectors are referenced by packages that can't be cleaned. If you have multiple games, it's recommended you revert any checked out Engine packages."));
		bUpdateEnginePackages=false;
	}

	UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT(""));

	/////////////////////////////////////////////////////////////////////
	// Load and save packages to save out the proper fixed up references
	/////////////////////////////////////////////////////////////////////

	SET_WARN_COLOR(COLOR_WHITE);
	UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Fixing references to point to proper objects:"));
	UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("---------------------------------------------"));
	CLEAR_WARN_COLOR();

	// keep a list of all packages that have ObjectRedirects in them. This is not needed if we aren't deleting redirects
	TArray<bool> PackagesWithRedirects;

	// Delete these packages entirely because they are empty
	TArray<bool> EmptyPackagesToDelete;

	if ( bDeleteRedirects )
	{
		PackagesWithRedirects.AddZeroed(UpdatePackages.Num());
		EmptyPackagesToDelete.AddZeroed(PackageList.Num());
	}

	bool bSCCEnabled = ISourceControlModule::Get().IsEnabled();

	// Iterate over all packages, loading and saving to remove all references to ObjectRedirectors (can't delete the Redirects yet)
	for( int32 PackageIndex = 0; PackageIndex < UpdatePackages.Num(); PackageIndex++ )
	{
		const FString& Filename = UpdatePackages[PackageIndex];

		// we can't do anything with packages that are read-only or cant be edited due to source control (we don't want to fixup the redirects)
		if(bSCCEnabled)
		{
			FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(Filename), EStateCacheUsage::ForceUpdate);
			if(SourceControlState.IsValid() && !SourceControlState->CanEdit())
			{
				continue;
			}
		}
		else if(IFileManager::Get().IsReadOnly(*Filename))
		{
			continue;
		}
		
		// Assert if package couldn't be opened so we have no chance of messing up saving later packages.
		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Cleaning %s"), *Filename);
		UPackage* Package = LoadPackage( NULL, *Filename, LOAD_NoVerify );
		// load all string asset reference targets, and add fake redirectors for them
		GRedirectCollector.ResolveStringAssetReference();

		// if it failed to open, we have already dealt with quitting if we are going to, so just skip it
		if (!Package)
		{
			continue;
		}

		if ( bDeleteOnlyReferencedRedirects && bDeleteRedirects )
		{
			TArray<UObject *> ObjectsInOuter;
			GetObjectsWithOuter(Package, ObjectsInOuter);
			for( int32 Index = 0; Index < ObjectsInOuter.Num(); Index++ )
			{
				UObject* Obj = ObjectsInOuter[Index];
				UObjectRedirector* Redir = Cast<UObjectRedirector>(Obj);
				if (Redir)
				{
					PackagesWithRedirects[PackageIndex] = 1;
					break;
				}
			}
		}

		// Don't save packages in the engine directory if we did not opt to update engine packages
		if( bUpdateEnginePackages || !Filename.StartsWith( FPaths::EngineDir() ) )
		{
			// save out the package
			UWorld* World = UWorld::FindWorldInPackage(Package);
			GEditor->SavePackage( Package, World, World ? RF_NoFlags : RF_Standalone, *Filename, GWarn );
		}
		
		// collect garbage to close the package
		UE_LOG(LogFixupRedirectsCommandlet, Display, TEXT("Collecting Garbage..."));
		CollectGarbage(RF_Native);
	}

	UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT(""));

	if ( bDeleteRedirects )
	{
		/////////////////////////////////////////////////////////////////////
		// Delete all redirects that are no longer referenced
		/////////////////////////////////////////////////////////////////////

		SET_WARN_COLOR(COLOR_WHITE);
		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Deleting redirector objects:"));
		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("----------------------------"));
		CLEAR_WARN_COLOR();

		const TArray<FString>& PackagesToCleaseOfRedirects = bDeleteOnlyReferencedRedirects ? UpdatePackages : PackageList;

		int32 GCIndex = 0;

		// Again, iterate over all packages, loading and saving to and this time deleting all 
		for( int32 PackageIndex = 0; PackageIndex < PackagesToCleaseOfRedirects.Num(); PackageIndex++ )
		{
			const FString& Filename = PackagesToCleaseOfRedirects[PackageIndex];
		
			if (bDeleteOnlyReferencedRedirects && PackagesWithRedirects[PackageIndex] == 0)
			{
				continue;
			}

			// The file should have already been checked out/be writable
			// If it is read only that means the checkout failed or we are not using source control and we can not write this file.
			if(bSCCEnabled)
			{
				FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(Filename), EStateCacheUsage::ForceUpdate);
				if(SourceControlState.IsValid() && !SourceControlState->CanEdit())
				{
					UE_LOG(LogFixupRedirectsCommandlet, Display, TEXT("Skipping file: source control says we cant edit the file %s..."), *Filename);
					continue;
				}
			}
			else if( IFileManager::Get().IsReadOnly( *Filename ))
			{
				UE_LOG(LogFixupRedirectsCommandlet, Display, TEXT("Skipping read-only file %s..."), *Filename);
				continue;
			}

			UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Wiping %s..."), *Filename);
		
			UPackage* Package = LoadPackage( NULL, *Filename, 0 );
			// load all string asset reference targets, and add fake redirectors for them
			GRedirectCollector.ResolveStringAssetReference();
			// if it failed to open, we have already dealt with quitting if we are going to, so just skip it
			if (!Package)
			{
				continue;
			}

			// assume that all redirs in this file are still-referenced
			bool bIsDirty = false;

			// see if this package is completely empty other than purged redirectors and metadata
			bool bIsEmpty = true;

			// delete all ObjectRedirects now
			TArray<UObjectRedirector*> Redirs;
			// find all redirectors, put into an array so delete doesn't mess up the iterator
			TArray<UObject *> ObjectsInOuter;
			GetObjectsWithOuter(Package, ObjectsInOuter);
			for( int32 Index = 0; Index < ObjectsInOuter.Num(); Index++ )
			{
				UObject* Obj = ObjectsInOuter[Index];
				UObjectRedirector *Redir = Cast<UObjectRedirector>(Obj);

				if (Redir)
				{
					int32 Dummy;
					// if the redirector was marked as uncleanable, skip it
					if (RedirectorsThatCantBeCleaned.Find(Redir->GetFullName(), Dummy))
					{
						SET_WARN_COLOR(COLOR_DARK_RED);
						UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("   ... skipping still-referenced %s"), *Redir->GetFullName());
						CLEAR_WARN_COLOR();
						bIsEmpty = false;
						continue;
					}

					bIsDirty = true;
					SET_WARN_COLOR(COLOR_DARK_GREEN);
					UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("   ... deleting %s"), *Redir->GetFullName());
					CLEAR_WARN_COLOR();
					Redirs.Add(Redir);
				}
				else if ( (!Obj->IsA(UMetaData::StaticClass()) && !Obj->IsA(UField::StaticClass()))
					|| Obj->IsA(UUserDefinedEnum::StaticClass())) // UserDefinedEnum should not be deleted
				{
					// This package has a real object
					bIsEmpty = false;
				}
			}

			if (bIsEmpty && !bDeleteOnlyReferencedRedirects)
			{
				EmptyPackagesToDelete[PackageIndex] = 1;
			}

			// mark them for deletion.
			for (int32 RedirIndex = 0; RedirIndex < Redirs.Num(); RedirIndex++)
			{
				UObjectRedirector* Redirector = Redirs[RedirIndex];
				// Remove standalone flag and mark as pending kill.
				Redirector->ClearFlags( RF_Standalone | RF_Public );
			
				// We don't need redirectors in the root set, the references should already be fixed up
				if ( Redirector->IsRooted() )
				{
					Redirector->RemoveFromRoot();
				}

				Redirector->MarkPendingKill();
			}
			Redirs.Empty();

			if (bIsDirty)
			{
				// Don't save packages in the engine directory if we did not opt to update engine packages
				if( bUpdateEnginePackages || !Filename.StartsWith( FPaths::EngineDir() ) )
				{
					// save the package
					UWorld* World = UWorld::FindWorldInPackage(Package);
					GEditor->SavePackage( Package, World, World ? RF_NoFlags : RF_Standalone, *Filename, GWarn );
				}
			}

			// collect garbage every N packages, or if there was any redirectors in the package 
			if (((++GCIndex) % 50) == 0 || bIsDirty || Package->ContainsMap())
			{
				// collect garbage to close the package
				CollectGarbage(RF_Native);
				UE_LOG(LogFixupRedirectsCommandlet, Display, TEXT("GC..."));
				// reset our counter
				GCIndex = 0;
			}
		}

		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT(""));
	}
	else
	{
		SET_WARN_COLOR(COLOR_WHITE);
		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("----------------------------"));
		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Not deleting any redirector objects:"));
		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("----------------------------"));
		UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT(""));
		CLEAR_WARN_COLOR();
	}

	/////////////////////////////////////////////////////////////////////
	// Clean up any unused trash
	/////////////////////////////////////////////////////////////////////

	SET_WARN_COLOR(COLOR_WHITE);
	UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Deleting empty and unused packages:"));
	UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("---------------------------------------"));
	CLEAR_WARN_COLOR();

	LogOutputFile->Logf(TEXT(""));
	LogOutputFile->Logf(TEXT("[Deleted]"));

	// Iterate over packages, attempting to delete unreferenced packages
	for( int32 PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
	{
		bool bDelete = false;
		const FString& Filename = PackageList[PackageIndex];
		
		FString PackageName(FPackageName::FilenameToLongPackageName(Filename));

		const bool bAllowDelete = bUpdateEnginePackages || !Filename.StartsWith( FPaths::EngineDir() );
		if (bDeleteRedirects && !bDeleteOnlyReferencedRedirects && EmptyPackagesToDelete[PackageIndex] && bAllowDelete)
		{
			// this package is completely empty, delete it
			bDelete = true;
		}

		if (bDelete == true)
		{
			struct Local
			{
				static bool DeleteFromDisk(const FString& InFilename, const FString& InMessage)
				{
					SET_WARN_COLOR(COLOR_GREEN);
					UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("%s"), *InMessage);
					if (!IFileManager::Get().Delete(*InFilename, false, true))
					{
						SET_WARN_COLOR(COLOR_RED);
						UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("  ... failed to delete from disk."), *InFilename);
						CLEAR_WARN_COLOR();
						return false;
					}
					CLEAR_WARN_COLOR();
					return true;
				}
			};

			if (!bIsSCCDisabled)
			{
				// get file SCC status
				FString FileName = SourceControlHelpers::PackageFilename(PackageName);
				FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(FileName, EStateCacheUsage::ForceUpdate);

				if(SourceControlState.IsValid() && (SourceControlState->IsCheckedOut() || SourceControlState->IsAdded()) )
				{
					UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Revert '%s' from source control..."), *Filename);
					SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), FileName);

					SET_WARN_COLOR(COLOR_GREEN);
					UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Deleting '%s' from source control..."), *Filename);
					CLEAR_WARN_COLOR();
					SourceControlProvider.Execute(ISourceControlOperation::Create<FDelete>(), FileName);
				}
				else if(SourceControlState.IsValid() && SourceControlState->CanCheckout())
				{
					SET_WARN_COLOR(COLOR_GREEN);
					UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Deleting '%s' from source control..."), *Filename);
					CLEAR_WARN_COLOR();
					SourceControlProvider.Execute(ISourceControlOperation::Create<FDelete>(), FileName);
				}
				else if(SourceControlState.IsValid() && SourceControlState->IsCheckedOutOther())
				{
					SET_WARN_COLOR(COLOR_RED);
					UE_LOG(LogFixupRedirectsCommandlet, Warning, TEXT("Couldn't delete '%s' from source control, someone has it checked out, skipping..."), *Filename);
					CLEAR_WARN_COLOR();
				}
				else if(SourceControlState.IsValid() && !SourceControlState->IsSourceControlled())
				{
					if(Local::DeleteFromDisk(Filename, FString::Printf(TEXT("'%s' is not in source control, attempting to delete from disk..."), *Filename)))
					{
						LogOutputFile->Logf(*Filename);
					}
				}
				else
				{
					if(Local::DeleteFromDisk(Filename, FString::Printf(TEXT("'%s' is in an unknown source control state, attempting to delete from disk..."), *Filename)))
					{
						LogOutputFile->Logf(*Filename);
					}
				}
			}
			else
			{
				if(Local::DeleteFromDisk(Filename, FString::Printf(TEXT("source control disabled while deleting '%s', attempting to delete from disk..."), *Filename)))
				{
					LogOutputFile->Logf(*Filename);
				}
			}
		}
	}
	LogOutputFile->Close();
	delete LogOutputFile;
	LogOutputFile = NULL;

	if (!bIsSCCDisabled && bAutoSubmit)
	{
		/////////////////////////////////////////////////////////////////////
		// Submit the results to source control
		/////////////////////////////////////////////////////////////////////
		UE_LOG(LogFixupRedirectsCommandlet, Display, TEXT("Submiting the results to source control"));

		// Work out the list of file to check in
		TArray <FString> FilesToSubmit;

		for( int32 PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
		{
			const FString& Filename = PackageList[PackageIndex];

			FString PackageName(FPackageName::FilenameToLongPackageName(Filename));
			FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(Filename), EStateCacheUsage::ForceUpdate);

			if( SourceControlState.IsValid() && (SourceControlState->IsCheckedOut() || SourceControlState->IsAdded() || SourceControlState->IsDeleted()) )
			{
				// Only submit engine packages if we're requested to
				if( bUpdateEnginePackages || !Filename.StartsWith( FPaths::EngineDir() ) )
				{
					FilesToSubmit.Add(*PackageName);
				}
			}
		}

		// Check in all changed files
		const FText Description = NSLOCTEXT("FixupRedirectsCmdlet", "ChangelistDescription", "Fixed up Redirects");
		TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
		CheckInOperation->SetDescription( Description );
		SourceControlProvider.Execute(CheckInOperation, SourceControlHelpers::PackageFilenames(FilesToSubmit));

		// toss the SCC manager
		ISourceControlModule::Get().GetProvider().Close();
	}
	return 0;
}
