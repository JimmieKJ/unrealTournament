// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TranslationEditorPrivatePCH.h"
#include "TranslationEditor.h"
#include "WorkspaceMenuStructureModule.h"
#include "TranslationUnit.h"
#include "ISourceControlModule.h"
#include "MessageLog.h"
#include "TextLocalizationManager.h"
#include "JsonInternationalizationArchiveSerializer.h"
#include "JsonInternationalizationManifestSerializer.h"
#include "SNotificationList.h"
#include "NotificationManager.h"


DEFINE_LOG_CATEGORY_STATIC(LogTranslationEditor, Log, All);

#define LOCTEXT_NAMESPACE "TranslationDataManager"


FTranslationDataManager::FTranslationDataManager( const FString& InManifestFilePath, const FString& InArchiveFilePath )
: ManifestFilePath(InManifestFilePath)
, ArchiveFilePath(InArchiveFilePath)
{
	GWarn->BeginSlowTask(LOCTEXT("LoadingTranslationData", "Loading Translation Data..."), true);
	TArray<UTranslationUnit*> TranslationUnits;

	ManifestAtHeadRevisionPtr = ReadManifest( ManifestFilePath );
	if (ManifestAtHeadRevisionPtr.IsValid())
	{
		TSharedRef< FInternationalizationManifest > ManifestAtHeadRevision = ManifestAtHeadRevisionPtr.ToSharedRef();
		int32 ManifestEntriesCount = ManifestAtHeadRevision->GetNumEntriesBySourceText();

		if (ManifestEntriesCount < 1)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add( TEXT("ManifestFilePath"), FText::FromString(ManifestFilePath) );
			Arguments.Add( TEXT("ManifestEntriesCount"), FText::AsNumber(ManifestEntriesCount) );
			FMessageLog TranslationEditorMessageLog("TranslationEditor");
			TranslationEditorMessageLog.Error(FText::Format(LOCTEXT("CurrentManifestEmpty", "Most current translation manifest ({ManifestFilePath}) has {ManifestEntriesCount} entries."), Arguments));
			TranslationEditorMessageLog.Notify(LOCTEXT("TranslationLoadError", "Error Loading Translations!"));
			TranslationEditorMessageLog.Open(EMessageSeverity::Error);
		}

		ArchivePtr = ReadArchive();
		if (ArchivePtr.IsValid())
		{
			int32 NumManifestEntriesParsed = 0;

			GWarn->BeginSlowTask(LOCTEXT("LoadingCurrentManifest", "Loading Entries from Current Translation Manifest..."), true);
			// Get all manifest entries by source text (same source text in multiple contexts will only show up once)
			for (auto ManifestItr = ManifestAtHeadRevision->GetEntriesBySourceTextIterator(); ManifestItr; ++ManifestItr, ++NumManifestEntriesParsed)
			{
				GWarn->StatusUpdate(NumManifestEntriesParsed, ManifestEntriesCount, FText::Format(LOCTEXT("LoadingCurrentManifestEntries", "Loading Entry {0} of {1} from Current Translation Manifest..."), FText::AsNumber(NumManifestEntriesParsed), FText::AsNumber(ManifestEntriesCount)));
				const TSharedRef<FManifestEntry> ManifestEntry = ManifestItr.Value();
				UTranslationUnit* TranslationUnit = NewObject<UTranslationUnit>();
				check(TranslationUnit != nullptr);
				// We want Undo/Redo support
				TranslationUnit->SetFlags(RF_Transactional);
				TranslationUnit->HasBeenReviewed = false;
				TranslationUnit->Source = ManifestEntry->Source.Text;
				TranslationUnit->Namespace = ManifestEntry->Namespace;

				for(auto ContextIter( ManifestEntry->Contexts.CreateConstIterator() ); ContextIter; ++ContextIter)
				{
					FTranslationContextInfo ContextInfo;
					const FContext& AContext = *ContextIter;

					ContextInfo.Context = AContext.SourceLocation;
					ContextInfo.Key = AContext.Key;

					TranslationUnit->Contexts.Add(ContextInfo);
				}

				TranslationUnits.Add(TranslationUnit);
			}
			GWarn->EndSlowTask();

			LoadFromArchive(TranslationUnits);
		}
		else  // ArchivePtr.IsValid() is false
		{
			FFormatNamedArguments Arguments;
			Arguments.Add( TEXT("ArchiveFilePath"), FText::FromString(ArchiveFilePath) );
			FMessageLog TranslationEditorMessageLog("TranslationEditor");
			TranslationEditorMessageLog.Error(FText::Format(LOCTEXT("FailedToLoadCurrentArchive", "Failed to load most current translation archive ({ArchiveFilePath}), unable to load translations."), Arguments));
			TranslationEditorMessageLog.Notify(LOCTEXT("TranslationLoadError", "Error Loading Translations!"));
			TranslationEditorMessageLog.Open(EMessageSeverity::Error);
		}
	}
	else  // ManifestAtHeadRevisionPtr.IsValid() is false
	{
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("ManifestFilePath"), FText::FromString(ManifestFilePath) );
		FMessageLog TranslationEditorMessageLog("TranslationEditor");
		TranslationEditorMessageLog.Error(FText::Format(LOCTEXT("FailedToLoadCurrentManifest", "Failed to load most current translation manifest ({ManifestFilePath}), unable to load translations."), Arguments));
		TranslationEditorMessageLog.Notify(LOCTEXT("TranslationLoadError", "Error Loading Translations!"));
		TranslationEditorMessageLog.Open(EMessageSeverity::Error);
	}

	GWarn->EndSlowTask();
}

FTranslationDataManager::~FTranslationDataManager()
{
	RemoveTranslationUnitArrayfromRoot(AllTranslations); // Re-enable garbage collection for all current UTranslationDataObjects
}

TSharedPtr< FInternationalizationManifest > FTranslationDataManager::ReadManifest( const FString& ManifestFilePathToRead )
{
	
	TSharedPtr<FJsonObject> ManifestJsonObject = ReadJSONTextFile( ManifestFilePathToRead );

	if( !ManifestJsonObject.IsValid() )
	{
		UE_LOG(LogTranslationEditor, Error, TEXT("Could not read manifest file %s."), *ManifestFilePathToRead);
		return TSharedPtr< FInternationalizationManifest >();
	}

	TSharedRef< FInternationalizationManifest > InternationalizationManifest = MakeShareable( new FInternationalizationManifest );

	ManifestSerializer.DeserializeManifest( ManifestJsonObject.ToSharedRef(), InternationalizationManifest );

	return InternationalizationManifest;
}

TSharedPtr< FInternationalizationArchive > FTranslationDataManager::ReadArchive()
{
	// Read in any existing archive for this culture.
	TSharedPtr< FJsonObject > ArchiveJsonObject = ReadJSONTextFile( ArchiveFilePath );

	if ( !ArchiveJsonObject.IsValid() )
	{
		UE_LOG(LogTranslationEditor, Error, TEXT("Could not read archive file %s."), *ArchiveFilePath);
		return nullptr;
	}

	TSharedRef< FInternationalizationArchive > InternationalizationArchive = MakeShareable( new FInternationalizationArchive );

	ArchiveSerializer.DeserializeArchive( ArchiveJsonObject.ToSharedRef(), InternationalizationArchive );

	return InternationalizationArchive;
}

TSharedPtr<FJsonObject> FTranslationDataManager::ReadJSONTextFile(const FString& InFilePath)
{
	//read in file as string
	FString FileContents;
	if ( !FFileHelper::LoadFileToString(FileContents, *InFilePath) )
	{
		UE_LOG(LogTranslationEditor, Error,TEXT("Failed to load file %s."), *InFilePath);
		return nullptr;
	}

	//parse as JSON
	TSharedPtr<FJsonObject> JSONObject;

	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( FileContents );

	if( !FJsonSerializer::Deserialize( Reader, JSONObject ) || !JSONObject.IsValid())
	{
		UE_LOG(LogTranslationEditor, Error,TEXT("Invalid JSON in file %s."), *InFilePath);
		return nullptr;
	}

	return JSONObject;
}

bool FTranslationDataManager::WriteTranslationData(bool bForceWrite /*= false*/)
{
	check (ArchivePtr.IsValid());
	TSharedRef< FInternationalizationArchive > Archive = ArchivePtr.ToSharedRef();

	bool bNeedsWrite = false;

	for (UTranslationUnit* TranslationUnit : Untranslated)
	{
		if (TranslationUnit != nullptr)
		{
			const FLocItem SearchSource(TranslationUnit->Source);
			FString OldTranslation = Archive->FindEntryBySource(TranslationUnit->Namespace, SearchSource, nullptr)->Translation.Text;
			FString TranslationToWrite = TranslationUnit->Translation;
			if (!TranslationToWrite.Equals(OldTranslation))
			{
				Archive->SetTranslation(TranslationUnit->Namespace, TranslationUnit->Source, TranslationToWrite, nullptr);
				bNeedsWrite = true;
			}
		}
	}

	for (UTranslationUnit* TranslationUnit : Review)
	{
		if (TranslationUnit != nullptr)
		{
			const FLocItem SearchSource(TranslationUnit->Source);
			FString OldTranslation = Archive->FindEntryBySource(TranslationUnit->Namespace, SearchSource, nullptr)->Translation.Text;
			FString TranslationToWrite = TranslationUnit->Translation;
			if (TranslationUnit->HasBeenReviewed && !TranslationToWrite.Equals(OldTranslation))
			{
				Archive->SetTranslation(TranslationUnit->Namespace, TranslationUnit->Source, TranslationToWrite, nullptr);
				bNeedsWrite = true;
			}
		}
	}

	for (UTranslationUnit* TranslationUnit : Complete)
	{
		if (TranslationUnit != nullptr)
		{
			const FLocItem SearchSource(TranslationUnit->Source);
			FString OldTranslation = Archive->FindEntryBySource(TranslationUnit->Namespace, SearchSource, nullptr)->Translation.Text;
			FString TranslationToWrite = TranslationUnit->Translation;
			if (!TranslationToWrite.Equals(OldTranslation))
			{
				Archive->SetTranslation(TranslationUnit->Namespace, TranslationUnit->Source, TranslationToWrite, nullptr);
				bNeedsWrite = true;
			}
		}
	}

	bool bSuccess = true;

	if (bForceWrite || bNeedsWrite)
	{
		TSharedRef<FJsonObject> FinalArchiveJsonObj = MakeShareable( new FJsonObject );
		ArchiveSerializer.SerializeArchive( Archive, FinalArchiveJsonObj );

		bSuccess = WriteJSONToTextFile( FinalArchiveJsonObj, ArchiveFilePath );
	}

	return bSuccess;
}

bool FTranslationDataManager::WriteJSONToTextFile(TSharedRef<FJsonObject>& Output, const FString& Filename)
{
	bool CheckoutAndSaveWasSuccessful = true;
	bool bPreviouslyCheckedOut = false;

	// If the user specified a reference file - write the entries read from code to a ref file
	if ( !Filename.IsEmpty() )
	{
		// If source control is enabled, try to check out the file. Otherwise just try to write it
		if (ISourceControlModule::Get().IsEnabled())
		{
			// Already checked out?
			if (CheckedOutFiles.Contains(Filename))
			{
				bPreviouslyCheckedOut = true;
			}
			else if (!SourceControlHelpers::CheckOutFile(Filename))
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Filename"), FText::FromString(Filename));
				// Use Source Control Message Log here because there might be other useful information in that log for the user.
				FMessageLog SourceControlMessageLog("SourceControl");
				SourceControlMessageLog.Error(FText::Format(LOCTEXT("CheckoutFailed", "Check out of file '{Filename}' failed."), Arguments));
				SourceControlMessageLog.Notify(LOCTEXT("TranslationArchiveCheckoutFailed", "Failed to Check Out Translation Archive!"));
				SourceControlMessageLog.Open(EMessageSeverity::Error);
				CheckoutAndSaveWasSuccessful = false;
			}
			else
			{
				CheckedOutFiles.Add(Filename);
			}
		}

		if( CheckoutAndSaveWasSuccessful )
		{
			//Print the JSON data out to the ref file.
			FString OutputString;
			TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create( &OutputString );
			FJsonSerializer::Serialize( Output, Writer );

			if (!FFileHelper::SaveStringToFile(OutputString, *Filename, FFileHelper::EEncodingOptions::ForceUnicode))
			{
				// If we already checked out the file, but cannot write it, perhaps the user checked it in via perforce, so try to check it out again
				if (bPreviouslyCheckedOut)
				{
					bPreviouslyCheckedOut = false;

					if( !SourceControlHelpers::CheckOutFile(Filename) )
					{
						FFormatNamedArguments Arguments;
						Arguments.Add( TEXT("Filename"), FText::FromString(Filename) );
						// Use Source Control Message Log here because there might be other useful information in that log for the user.
						FMessageLog SourceControlMessageLog("SourceControl");
						SourceControlMessageLog.Error(FText::Format(LOCTEXT("CheckoutFailed", "Check out of file '{Filename}' failed."), Arguments));
						SourceControlMessageLog.Notify(LOCTEXT("TranslationArchiveCheckoutFailed", "Failed to Check Out Translation Archive!"));
						SourceControlMessageLog.Open(EMessageSeverity::Error);
						CheckoutAndSaveWasSuccessful = false;

						CheckedOutFiles.Remove(Filename);
					}
				}

				FFormatNamedArguments Arguments;
				Arguments.Add( TEXT("Filename"), FText::FromString(Filename) );
				FMessageLog TranslationEditorMessageLog("TranslationEditor");
				TranslationEditorMessageLog.Error(FText::Format(LOCTEXT("WriteFileFailed", "Failed to write localization entries to file '{Filename}'."), Arguments));
				TranslationEditorMessageLog.Notify(LOCTEXT("FileWriteFailed", "Failed to Write Translations to File!"));
				TranslationEditorMessageLog.Open(EMessageSeverity::Error);
				CheckoutAndSaveWasSuccessful = false;
			}
		}
	}
	else
	{
		CheckoutAndSaveWasSuccessful = false;
	}

	// If this is the first time, let the user know the file was checked out
	if (!bPreviouslyCheckedOut && CheckoutAndSaveWasSuccessful)
	{
		struct Local
		{
			/**
			* Called by our notification's hyperlink to open the Source Control message log
			*/
			static void OpenSourceControlMessageLog(  )
			{
				FMessageLog("SourceControl").Open();
			}
		};

		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("Filename"), FText::FromString(Filename) );

		// Make a note in the Source Control log, including a note to check in the file later via source control application
		FMessageLog TranslationEditorMessageLog("SourceControl");
		TranslationEditorMessageLog.Info(FText::Format(LOCTEXT("TranslationArchiveCheckedOut", "Successfully checked out and saved translation archive '{Filename}'. Please check-in this file later via your source control application."), Arguments));

		// Display notification that save was successful, along with a link to the Source Control log so the user can see the above message.
		FNotificationInfo Info( LOCTEXT("ArchiveCheckedOut", "Translation Archive Successfully Checked Out and Saved.") );
		Info.ExpireDuration = 5;
		Info.Hyperlink = FSimpleDelegate::CreateStatic(&Local::OpenSourceControlMessageLog);
		Info.HyperlinkText = LOCTEXT("ShowMessageLogHyperlink", "Show Message Log");
		Info.bFireAndForget = true;
		Info.bUseSuccessFailIcons = true;
		Info.Image = FEditorStyle::GetBrush(TEXT("NotificationList.SuccessImage"));
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	return CheckoutAndSaveWasSuccessful;
}

void FTranslationDataManager::GetHistoryForTranslationUnits()
{
	GWarn->BeginSlowTask(LOCTEXT("LoadingSourceControlHistory", "Loading Translation History from Source Control..."), true);

	TArray<UTranslationUnit*>& TranslationUnits = AllTranslations;
	const FString& InManifestFilePath = ManifestFilePath;

	// Unload any previous history information, going to retrieve it all again.
	UnloadHistoryInformation();

	// Force history update
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateStatusOperation->SetUpdateHistory( true );
	ECommandResult::Type Result = SourceControlProvider.Execute(UpdateStatusOperation, InManifestFilePath);
	bool bGetHistoryFromSourceControlSucceeded = Result == ECommandResult::Succeeded;

	// Now we can get information about the file's history from the source control state, retrieve that
	TArray<FString> Files;
	TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> > States;
	Files.Add(InManifestFilePath);
	Result = SourceControlProvider.GetState( Files,  States, EStateCacheUsage::ForceUpdate );
	bGetHistoryFromSourceControlSucceeded = bGetHistoryFromSourceControlSucceeded && (Result == ECommandResult::Succeeded);
	FSourceControlStatePtr SourceControlState;
	if (States.Num() == 1)
	{
		SourceControlState = States[0];
	}

	// If all the source control operations went ok, continue
	if (bGetHistoryFromSourceControlSucceeded && SourceControlState.IsValid())
	{
		int32 HistorySize = SourceControlState->GetHistorySize();

		for (int HistoryItemIndex = HistorySize-1; HistoryItemIndex >=0; --HistoryItemIndex)
		{
			GWarn->StatusUpdate(HistorySize - HistoryItemIndex, HistorySize, FText::Format(LOCTEXT("LoadingOldManifestRevisionNumber", "Loading Translation History from Manifest Revision {0} of {1} from Source Control..."), FText::AsNumber(HistorySize - HistoryItemIndex), FText::AsNumber(HistorySize)));

			TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision = SourceControlState->GetHistoryItem(HistoryItemIndex);
			if(Revision.IsValid())
			{
				FString ManifestFullPath = FPaths::ConvertRelativePathToFull(InManifestFilePath);
				FString EngineFullPath = FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir());

				bool IsEngineManifest = false;
				if (ManifestFullPath.StartsWith(EngineFullPath))
				{
					IsEngineManifest = true;
				}

				FString ProjectName;
				FString SavedDir;	// Store these cached translation history files in the saved directory
				if (IsEngineManifest)
				{
					ProjectName = "Engine";
					SavedDir = FPaths::EngineSavedDir();
				}
				else
				{
					ProjectName = FApp::GetGameName();
					SavedDir = FPaths::GameSavedDir();
				}

				FString TempFileName = SavedDir / "CachedTranslationHistory" / "UE4-Manifest-" + ProjectName + "-" + FPaths::GetBaseFilename(InManifestFilePath) + "-Rev-" + FString::FromInt(Revision->GetRevisionNumber());

				
				if (!FPaths::FileExists(TempFileName))	// Don't bother syncing again if we already have this manifest version cached locally
				{
					Revision->Get(TempFileName);
				}

				TSharedPtr< FInternationalizationManifest > OldManifestPtr = ReadManifest( TempFileName );
				if (OldManifestPtr.IsValid())	// There may be corrupt manifests in the history, so ignore them.
				{
					TSharedRef< FInternationalizationManifest > OldManifest = OldManifestPtr.ToSharedRef();

					for (UTranslationUnit* TranslationUnit : TranslationUnits)
					{
						if(TranslationUnit != nullptr && TranslationUnit->Contexts.Num() > 0)
						{
							for (FTranslationContextInfo& ContextInfo : TranslationUnit->Contexts)
							{
								FString PreviousSourceText = "";

								// If we already have history, then compare against the newest history so far
								if (ContextInfo.Changes.Num() > 0)
								{
									PreviousSourceText = ContextInfo.Changes[0].Source;
								}

								FContext SearchContext;
								SearchContext.Key = ContextInfo.Key;
								TSharedPtr< FManifestEntry > OldManifestEntryPtr = OldManifest->FindEntryByContext(TranslationUnit->Namespace, SearchContext);
								if (!OldManifestEntryPtr.IsValid())
								{
									// If this version of the manifest didn't know anything about this string, move onto the next
									continue;
								}

								// Always add first instance of this string, and then add any versions that changed since
								if (ContextInfo.Changes.Num() == 0 || !OldManifestEntryPtr->Source.Text.Equals(PreviousSourceText))
								{
									TSharedPtr< FArchiveEntry > OldArchiveEntry = ArchivePtr->FindEntryBySource(OldManifestEntryPtr->Namespace, OldManifestEntryPtr->Source, nullptr);
									if (OldArchiveEntry.IsValid())
									{
										FTranslationChange Change;
										Change.Source = OldManifestEntryPtr->Source.Text;
										Change.Translation = OldArchiveEntry->Translation.Text;
										Change.DateAndTime = Revision->GetDate();
										Change.Version = FString::FromInt(Revision->GetRevisionNumber());
										ContextInfo.Changes.Insert(Change, 0);
									}
								}
							}
						}
					}
				}
				else // OldManifestPtr.IsValid() is false
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("ManifestFilePath"), FText::FromString(InManifestFilePath));
					Arguments.Add( TEXT("ManifestRevisionNumber"), FText::AsNumber(Revision->GetRevisionNumber()) );
					FMessageLog TranslationEditorMessageLog("TranslationEditor");
					TranslationEditorMessageLog.Warning(FText::Format(LOCTEXT("PreviousManifestCorrupt", "Previous revision {ManifestRevisionNumber} of {ManifestFilePath} failed to load correctly. Ignoring."), Arguments));
				}
			}
		}
	}
	// If source control operations failed, display error message
	else // (bGetHistoryFromSourceControlSucceeded && SourceControlState.IsValid()) is false
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ManifestFilePath"), FText::FromString(InManifestFilePath));
		FMessageLog TranslationEditorMessageLog("SourceControl");
		TranslationEditorMessageLog.Warning(FText::Format(LOCTEXT("SourceControlStateQueryFailed", "Failed to query source control state of file {ManifestFilePath}."), Arguments));
		TranslationEditorMessageLog.Notify(LOCTEXT("RetrieveTranslationHistoryFailed", "Unable to Retrieve Translation History from Source Control!"));
	}


	// Go though all translation units
	for (int32 CurrentTranslationUnitIndex = 0; CurrentTranslationUnitIndex < TranslationUnits.Num(); ++CurrentTranslationUnitIndex)
	{
		UTranslationUnit* TranslationUnit = TranslationUnits[CurrentTranslationUnitIndex];
		if (TranslationUnit != nullptr)
		{
			if (TranslationUnit->Translation.IsEmpty())
			{
				bool bHasTranslationHistory = false;
				int32 MostRecentNonNullTranslationIndex = -1;
				int32 ContextForRecentTranslation = -1;

				// Check all contexts for history
				for (int32 ContextIndex = 0; ContextIndex < TranslationUnit->Contexts.Num(); ++ContextIndex)
				{
					for (int32 ChangeIndex = 0; ChangeIndex < TranslationUnit->Contexts[ContextIndex].Changes.Num(); ++ChangeIndex)
					{
						if (!(TranslationUnit->Contexts[ContextIndex].Changes[ChangeIndex].Translation.IsEmpty()))
						{
							bHasTranslationHistory = true;
							MostRecentNonNullTranslationIndex = ChangeIndex;
							ContextForRecentTranslation = ContextIndex;
							break;
						}
					}

					if (bHasTranslationHistory)
					{
						break;
					}
				}

				// If we have history, but current translation is empty, this goes in the Needs Review tab
				if (bHasTranslationHistory)
				{
					// Offer the most recent translation (for the first context in the list) as a suggestion or starting point (not saved unless user checks "Has Been Reviewed")
					TranslationUnit->Translation = TranslationUnit->Contexts[ContextForRecentTranslation].Changes[MostRecentNonNullTranslationIndex].Translation;
					TranslationUnit->HasBeenReviewed = false;

					// Move from Untranslated to review
					if (Untranslated.Contains(TranslationUnit))
					{
						Untranslated.Remove(TranslationUnit);
					}
					if (!Review.Contains(TranslationUnit))
					{
						Review.Add(TranslationUnit);
					}
				}
			}
		}
	}


	GWarn->EndSlowTask();
}

void FTranslationDataManager::HandlePropertyChanged(FName PropertyName)
{
	// When a property changes, write the data so we don't lose changes if user forgets to save or editor crashes
	WriteTranslationData();
}

void FTranslationDataManager::PreviewAllTranslationsInEditor()
{
	FString ManifestFullPath = FPaths::ConvertRelativePathToFull(ManifestFilePath);
	FString EngineFullPath = FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir());

	bool IsEngineManifest = false;
	if (ManifestFullPath.StartsWith(EngineFullPath))
	{
		IsEngineManifest = true;
	}

	FString ConfigDirectory;
	if (IsEngineManifest)
	{
		ConfigDirectory = FPaths::EngineConfigDir();
	}
	else
	{
		ConfigDirectory = FPaths::GameConfigDir();
	}

	FString ConfigFilePath = ConfigDirectory / "Localization" / "Regenerate" + FPaths::GetBaseFilename(ManifestFilePath) + ".ini";

	FJsonInternationalizationArchiveSerializer LocalizationArchiveSerializer;
	FJsonInternationalizationManifestSerializer LocalizationManifestSerializer;

	FTextLocalizationManager::Get().LoadFromManifestAndArchives(ConfigFilePath, LocalizationArchiveSerializer, LocalizationManifestSerializer);
}

void FTranslationDataManager::PopulateSearchResultsUsingFilter(const FString& SearchFilter)
{
	SearchResults.Empty();

	for (UTranslationUnit* TranslationUnit : AllTranslations)
	{
		if (TranslationUnit != nullptr)
		{
			bool bAdded = false;
			if (TranslationUnit->Source.Contains(SearchFilter) ||
				TranslationUnit->Translation.Contains(SearchFilter) || 
				TranslationUnit->Namespace.Contains(SearchFilter))
			{
				SearchResults.Add(TranslationUnit);
				bAdded = true;
			}
			
			for (FTranslationContextInfo CurrentContext : TranslationUnit->Contexts)
			{
				if (!bAdded && 
					(CurrentContext.Context.Contains(SearchFilter) ||
					CurrentContext.Key.Contains(SearchFilter)))
				{
					SearchResults.Add(TranslationUnit);
					break;
				}
			}
		}
	}
}

void FTranslationDataManager::LoadFromArchive(TArray<UTranslationUnit*>& InTranslationUnits, bool bTrackChanges /*= false*/, bool bReloadFromFile /*=false*/)
{
	GWarn->BeginSlowTask(LOCTEXT("LoadingArchiveEntries", "Loading Entries from Translation Archive..."), true);

	if (bReloadFromFile)
	{
		ArchivePtr = ReadArchive();
	}

	if (ArchivePtr.IsValid())
	{
		TSharedRef< FInternationalizationArchive > Archive = ArchivePtr.ToSharedRef();

		// Make a local copy of this array before we empty the arrays below (we might have been passed AllTranslations array)
		TArray<UTranslationUnit*> TranslationUnits;
		TranslationUnits.Append(InTranslationUnits);

		AllTranslations.Empty();
		Untranslated.Empty();
		Review.Empty();
		Complete.Empty();
		ChangedOnImport.Empty();

		for (int32 CurrentTranslationUnitIndex = 0; CurrentTranslationUnitIndex < TranslationUnits.Num(); ++CurrentTranslationUnitIndex)
		{
			UTranslationUnit* TranslationUnit = TranslationUnits[CurrentTranslationUnitIndex];
			if (TranslationUnit != nullptr)
			{
				if (!TranslationUnit->IsRooted())
				{
					TranslationUnit->AddToRoot(); // Disable garbage collection for UTranslationUnit objects
				}
				AllTranslations.Add(TranslationUnit);

				GWarn->StatusUpdate(CurrentTranslationUnitIndex, TranslationUnits.Num(), FText::Format(LOCTEXT("LoadingCurrentArchiveEntries", "Loading Entry {0} of {1} from Translation Archive..."), FText::AsNumber(CurrentTranslationUnitIndex), FText::AsNumber(TranslationUnits.Num())));

				const FLocItem SourceSearch(TranslationUnit->Source);
				TSharedPtr<FArchiveEntry> ArchiveEntry = Archive->FindEntryBySource(TranslationUnit->Namespace, SourceSearch, nullptr);
				if (ArchiveEntry.IsValid())
				{
					const FString PreviousTranslation = TranslationUnit->Translation;
					TranslationUnit->Translation = ""; // Reset to null string
					const FString TranslatedString = ArchiveEntry->Translation.Text;

					if (TranslatedString.IsEmpty())
					{
						bool bHasTranslationHistory = false;
						int32 MostRecentNonNullTranslationIndex = -1;
						int32 ContextForRecentTranslation = -1;

						for (int32 ContextIndex = 0; ContextIndex < TranslationUnit->Contexts.Num(); ++ContextIndex)
						{
							for (int32 ChangeIndex = 0; ChangeIndex < TranslationUnit->Contexts[ContextIndex].Changes.Num(); ++ChangeIndex)
							{
								if (!(TranslationUnit->Contexts[ContextIndex].Changes[ChangeIndex].Translation.IsEmpty()))
								{
									bHasTranslationHistory = true;
									MostRecentNonNullTranslationIndex = ChangeIndex;
									ContextForRecentTranslation = ContextIndex;
									break;
								}
							}

							if (bHasTranslationHistory)
							{
								break;
							}
						}

						// If we have history, but current translation is empty, this goes in the Needs Review tab
						if (bHasTranslationHistory)
						{
							// Offer the most recent translation (for the first context in the list) as a suggestion or starting point (not saved unless user checks "Has Been Reviewed")
							TranslationUnit->Translation = TranslationUnit->Contexts[ContextForRecentTranslation].Changes[MostRecentNonNullTranslationIndex].Translation;
							Review.Add(TranslationUnit);
						}
						else
						{
							Untranslated.Add(TranslationUnit);
						}
					}
					else
					{
						TranslationUnit->Translation = TranslatedString;
						TranslationUnit->HasBeenReviewed = true;
						Complete.Add(TranslationUnit);
					}

					// Add to changed array if we're tracking changes (i.e. when we import from .po files)
					if (bTrackChanges)
					{
						if (PreviousTranslation != TranslationUnit->Translation)
						{
							FString PreviousTranslationTrimmed = PreviousTranslation;
							PreviousTranslationTrimmed.Trim().TrimTrailing();
							FString CurrentTranslationTrimmed = TranslationUnit->Translation;
							CurrentTranslationTrimmed.Trim().TrimTrailing();
							// Ignore changes to only whitespace at beginning and/or end of string on import
							if (PreviousTranslationTrimmed == CurrentTranslationTrimmed)
							{
								TranslationUnit->Translation = PreviousTranslation;
							}
							else
							{
								ChangedOnImport.Add(TranslationUnit);
								TranslationUnit->TranslationBeforeImport = PreviousTranslation;
							}
						}
					}
				}
			}
		}
	}
	else  // ArchivePtr.IsValid() is false
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ArchiveFilePath"), FText::FromString(ArchiveFilePath));
		FMessageLog TranslationEditorMessageLog("TranslationEditor");
		TranslationEditorMessageLog.Error(FText::Format(LOCTEXT("FailedToLoadCurrentArchive", "Failed to load most current translation archive ({ArchiveFilePath}), unable to load translations."), Arguments));
		TranslationEditorMessageLog.Notify(LOCTEXT("TranslationLoadError", "Error Loading Translations!"));
		TranslationEditorMessageLog.Open(EMessageSeverity::Error);
	}
	
	GWarn->EndSlowTask();
}

void FTranslationDataManager::RemoveTranslationUnitArrayfromRoot(TArray<UTranslationUnit*>& TranslationUnits)
{
	for (UTranslationUnit* TranslationUnit : TranslationUnits)
	{
		TranslationUnit->RemoveFromRoot();
	}
}

void FTranslationDataManager::UnloadHistoryInformation()
{
	TArray<UTranslationUnit*>& TranslationUnits = AllTranslations;

	for (int32 CurrentTranslationUnitIndex = 0; CurrentTranslationUnitIndex < TranslationUnits.Num(); ++CurrentTranslationUnitIndex)
	{
		UTranslationUnit* TranslationUnit = TranslationUnits[CurrentTranslationUnitIndex];
		if (TranslationUnit != nullptr)
		{
			// If HasBeenReviewed is false, this is a suggestion translation from a previous translation for the same Namespace/Key pair
			if (!TranslationUnit->HasBeenReviewed)
			{
				if (!Untranslated.Contains(TranslationUnit))
				{
					Untranslated.Add(TranslationUnit);
				}
				if (Review.Contains(TranslationUnit))
				{
					Review.Remove(TranslationUnit);
				}

				// Erase previously suggested translation from history (it has not been reviewed)
				TranslationUnit->Translation.Empty();

				// Remove all history entries
				for (FTranslationContextInfo Context : TranslationUnit->Contexts)
				{
					Context.Changes.Empty();
				}
			}
		}
	}
}

bool FTranslationDataManager::SaveSelectedTranslations(TArray<UTranslationUnit*> TranslationUnitsToSave)
{
	bool bSucceeded = true;
	
	TMap<FString, TSharedPtr<TArray<UTranslationUnit*>>> TextsToSavePerProject;

	// Regroup the translations to save by project
	for (UTranslationUnit* TextToSave : TranslationUnitsToSave)
	{
		FString LocresFilePath = TextToSave->LocresPath;
		if (!LocresFilePath.IsEmpty())
		{
			if (!TextsToSavePerProject.Contains(LocresFilePath))
			{
				TextsToSavePerProject.Add(LocresFilePath, MakeShareable(new TArray<UTranslationUnit*>()));
			}

			TSharedPtr<TArray<UTranslationUnit*>> ProjectArray = TextsToSavePerProject.FindRef(LocresFilePath);
			ProjectArray->Add(TextToSave);
		}
	}

	for (auto TextIt = TextsToSavePerProject.CreateIterator(); TextIt; ++TextIt)
	{
		auto Item = *TextIt;
		FString CurrentLocResPath = Item.Key;
		FString ManifestAndArchiveName = FPaths::GetBaseFilename(CurrentLocResPath);
		FString ArchiveFilePath = FPaths::GetPath(CurrentLocResPath);
		FString CultureName = FPaths::GetBaseFilename(ArchiveFilePath);
		FString ManifestPath = FPaths::GetPath(ArchiveFilePath);
		FString ArchiveFullPath = ArchiveFilePath / ManifestAndArchiveName + ".archive";
		FString ManifestFullPath = ManifestPath / ManifestAndArchiveName + ".manifest";

		if (FPaths::FileExists(ManifestFullPath) && FPaths::FileExists(ArchiveFullPath))
		{
			TSharedRef<FTranslationDataManager> DataManager = MakeShareable(new FTranslationDataManager(ManifestFullPath, ArchiveFullPath));

			TArray<UTranslationUnit*>& TranslationsArray = DataManager->GetAllTranslationsArray();
			TSharedPtr<TArray<UTranslationUnit*>> EditedItems = Item.Value;

			// For each edited item belonging to this manifest/archive pair
			for (auto EditedItemIt = EditedItems->CreateIterator(); EditedItemIt; ++EditedItemIt)
			{
				UTranslationUnit* EditedItem = *EditedItemIt;

				// Search all translations for the one that matches this FText
				for (UTranslationUnit* Translation : TranslationsArray)
				{
					// If namespace matches...
					if (Translation->Namespace == EditedItem->Namespace)
					{
						// And source matches
						if (Translation->Source == EditedItem->Source)
						{
							// Update the translation in TranslationDataManager, and finish searching these translations
							Translation->Translation = EditedItem->Translation;
							break;
						}
					}
				}
			}

			// Save the data to file, and preview in editor
			bSucceeded = bSucceeded && DataManager->WriteTranslationData();
			DataManager->PreviewAllTranslationsInEditor();
		}
		else
		{
			bSucceeded = false;
		}
	}

	return bSucceeded;
}

#undef LOCTEXT_NAMESPACE
