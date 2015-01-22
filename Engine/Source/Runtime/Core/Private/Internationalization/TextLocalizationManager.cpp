// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "Misc/App.h"
#include "InternationalizationManifest.h"
#include "TextLocalizationResourceGenerator.h"


DEFINE_LOG_CATEGORY_STATIC(LogTextLocalizationManager, Log, All);

static FString AccessedStringBeforeLocLoadedErrorMsg = TEXT("Can't access string. Loc System hasn't been initialized yet!");

void BeginInitTextLocalization()
{
	// Initialize FInternationalization before we bind to OnCultureChanged otherwise we can accidentally initialize
	// twice since FInternationalization::Initialize sets the culture
	FInternationalization::Get();

	FInternationalization::Get().OnCultureChanged().AddRaw( &(FTextLocalizationManager::Get()), &FTextLocalizationManager::OnCultureChanged );
}

void EndInitTextLocalization()
{
	const bool ShouldLoadEditor = WITH_EDITOR;
	const bool ShouldLoadGame = FApp::IsGame();

	FInternationalization& I18N = FInternationalization::Get();

	// Set culture according to configuration now that configs are available.
#if ENABLE_LOC_TESTING
	if( FCommandLine::IsInitialized() && FParse::Param(FCommandLine::Get(), TEXT("LEET")) )
	{
		I18N.SetCurrentCulture(TEXT("LEET"));
	}
	else
#endif
	{
		FString RequestedCultureName;
		if (FParse::Value(FCommandLine::Get(), TEXT("CULTUREFORCOOKING="), RequestedCultureName))
		{
			// Write the culture passed in if first install...
			if (FParse::Param(FCommandLine::Get(), TEXT("firstinstall")))
			{
				GConfig->SetString(TEXT("Internationalization"), TEXT("Culture"), *RequestedCultureName, GEngineIni);
			}
		}
		else
#if !UE_BUILD_SHIPPING
		// Use culture override specified on commandline.
		if (FParse::Value(FCommandLine::Get(), TEXT("CULTURE="), RequestedCultureName))
		{
			//UE_LOG(LogInit, Log, TEXT("Overriding culture %s w/ command-line option".), *CultureName);
		}
		else
#endif // !UE_BUILD_SHIPPING
#if WITH_EDITOR
		// See if we've been provided a culture override in the editor
		if(GConfig->GetString( TEXT("Internationalization"), TEXT("Culture"), RequestedCultureName, GEditorGameAgnosticIni ))
		{
			//UE_LOG(LogInit, Log, TEXT("Overriding culture %s w/ editor configuration."), *CultureName);
		}
		else
#endif // WITH_EDITOR
		// Use culture specified in engine configuration.
		if(GConfig->GetString( TEXT("Internationalization"), TEXT("Culture"), RequestedCultureName, GEngineIni ))
		{
			//UE_LOG(LogInit, Log, TEXT("Overriding culture %s w/ engine configuration."), *CultureName);
		}
		else
		{
			RequestedCultureName = I18N.GetDefaultCulture()->GetName();
		}

		FString TargetCultureName = RequestedCultureName;
		{
			TArray<FString> LocalizationPaths;
			if(ShouldLoadEditor)
			{
				LocalizationPaths += FPaths::GetEditorLocalizationPaths();
			}
			if(ShouldLoadGame)
			{
				LocalizationPaths += FPaths::GetGameLocalizationPaths();
			}
			LocalizationPaths += FPaths::GetEngineLocalizationPaths();

			// Validate the locale has data or fallback to one that does.
			TArray< FCultureRef > AvailableCultures;
			I18N.GetCulturesWithAvailableLocalization(LocalizationPaths, AvailableCultures, false);

			TArray<FString> PrioritizedParentCultureNames = I18N.GetCurrentCulture()->GetPrioritizedParentCultureNames();
				
			FString ValidCultureName;
			for (const FString& CultureName : PrioritizedParentCultureNames)
			{
				FCulturePtr ValidCulture = I18N.GetCulture(CultureName);
				if (ValidCulture.IsValid() && AvailableCultures.Contains(ValidCulture.ToSharedRef()))
				{
					ValidCultureName = CultureName;
					break;
				}
			}

			if(!ValidCultureName.IsEmpty())
			{
				if(RequestedCultureName != ValidCultureName)
				{
					// Make the user aware that the localization data belongs to a parent culture.
					UE_LOG(LogTextLocalizationManager, Log, TEXT("The requested culture ('%s') has no localization data; parent culture's ('%s') localization data will be used."), *RequestedCultureName, *ValidCultureName);
				}
			}
			else
			{
				// Fallback to English.
				UE_LOG(LogTextLocalizationManager, Log, TEXT("The requested culture ('%s') has no localization data; falling back to 'en' for localization and internationalization data."), *RequestedCultureName);
				TargetCultureName = "en";
			}
		}

		I18N.SetCurrentCulture(TargetCultureName);
	}

	FTextLocalizationManager::Get().LoadResources(ShouldLoadEditor, ShouldLoadGame);
	FTextLocalizationManager::Get().bIsInitialized = true;
}

FTextLocalizationManager& FTextLocalizationManager::Get()
{
	static FTextLocalizationManager* GTextLocalizationManager = NULL;
	if( !GTextLocalizationManager )
	{
		GTextLocalizationManager = new FTextLocalizationManager;
	}

	return *GTextLocalizationManager;
}

const TSharedRef<FString, ESPMode::ThreadSafe>* FTextLocalizationManager::FTextLookupTable::GetString(const FString& Namespace, const FString& Key, const uint32 SourceStringHash) const
{
	const FKeyTable* const KeyTable = NamespaceTable.Find(Namespace);
	if( KeyTable )
	{
		const FStringEntry* const Entry = KeyTable->Find(Key);
		if( Entry )
		{
			return &(Entry->String);
		}
	}

	return NULL;
}

void FTextLocalizationManager::FLocalizationEntryTracker::ReportCollisions() const
{
	for(auto i = Namespaces.CreateConstIterator(); i; ++i)
	{
		const FString& NamespaceName = i.Key();
		const FLocalizationEntryTracker::FKeyTable& KeyTable = i.Value();
		for(auto j = KeyTable.CreateConstIterator(); j; ++j)
		{
			const FString& KeyName = j.Key();
			const FLocalizationEntryTracker::FEntryArray& EntryArray = j.Value();

			bool bWasCollisionDetected = false;
			for(int32 k = 0; k < EntryArray.Num(); ++k)
			{
				const FLocalizationEntryTracker::FEntry& LeftEntry = EntryArray[k];
				for(int32 l = k + 1; l < EntryArray.Num(); ++l)
				{
					const FLocalizationEntryTracker::FEntry& RightEntry = EntryArray[l];
					const bool bDoesSourceStringHashDiffer = LeftEntry.SourceStringHash != RightEntry.SourceStringHash;
					const bool bDoesLocalizedStringDiffer = LeftEntry.LocalizedString != RightEntry.LocalizedString;
					bWasCollisionDetected = bDoesSourceStringHashDiffer || bDoesLocalizedStringDiffer;
				}
			}

			if(bWasCollisionDetected)
			{
				FString CollidingEntryListString;
				for(int32 k = 0; k < EntryArray.Num(); ++k)
				{
					const FLocalizationEntryTracker::FEntry& Entry = EntryArray[k];

					if( !(CollidingEntryListString.IsEmpty()) )
					{
						CollidingEntryListString += '\n';
					}

					CollidingEntryListString += FString::Printf( TEXT("Resource: (%s) Hash: (%d) String: (%s)"), *(Entry.TableName), Entry.SourceStringHash, *(Entry.LocalizedString) );
				}

				UE_LOG(LogTextLocalizationManager, Warning, TEXT("Loading localization resources contain conflicting entries for (Namespace:%s, Key:%s):\n%s"), *NamespaceName, *KeyName, *CollidingEntryListString);
			}
		}
	}
}

void FTextLocalizationManager::FLocalizationEntryTracker::ReadFromDirectory(const FString& DirectoryPath)
{
	// Find resources in the specified folder.
	TArray<FString> ResourceFileNames;
	IFileManager::Get().FindFiles(ResourceFileNames, *(DirectoryPath / TEXT("*.locres")), true, false);

	// For each resource:
	for(int32 ResourceIndex = 0; ResourceIndex < ResourceFileNames.Num(); ++ResourceIndex)
	{
		const FString ResourceFilePath = DirectoryPath / ResourceFileNames[ResourceIndex];
		ReadFromFile( FPaths::ConvertRelativePathToFull(ResourceFilePath) );
	}
}

bool FTextLocalizationManager::FLocalizationEntryTracker::ReadFromFile(const FString& FilePath)
{
	FArchive* Reader = IFileManager::Get().CreateFileReader( *FilePath );
	if( !Reader )
	{
		return false;
	}

	Reader->SetForceUnicode(true);

	ReadFromArchive(*Reader, FilePath);

	bool Success = Reader->Close();
	delete Reader;

	return Success;
}

void FTextLocalizationManager::FLocalizationEntryTracker::ReadFromArchive(FArchive& Archive, const FString& Identifier)
{
	// Read namespace count
	uint32 NamespaceCount;
	Archive << NamespaceCount;

	for(uint32 i = 0; i < NamespaceCount; ++i)
	{
		// Read namespace
		FString Namespace;
		Archive << Namespace;

		// Read key count
		uint32 KeyCount;
		Archive << KeyCount;

		FLocalizationEntryTracker::FKeyTable& KeyTable = Namespaces.FindOrAdd(*Namespace);

		for(uint32 j = 0; j < KeyCount; ++j)
		{
			// Read key
			FString Key;
			Archive << Key;

			FLocalizationEntryTracker::FEntryArray& EntryArray = KeyTable.FindOrAdd( *Key );

			FLocalizationEntryTracker::FEntry NewEntry;
			NewEntry.TableName = Identifier;

			// Read string entry.
			Archive << NewEntry.SourceStringHash;
			Archive << NewEntry.LocalizedString;

			EntryArray.Add(NewEntry);
		}
	}
}

void FTextLocalizationManager::LoadResources(const bool ShouldLoadEditor, const bool ShouldLoadGame)
{
	// Add one to the revision index, so all FText's refresh.
	++HeadCultureRevisionIndex;

	FInternationalization& I18N = FInternationalization::Get();

#if ENABLE_LOC_TESTING
	{
		const FString& CultureName = I18N.GetCurrentCulture()->GetName();

		if(CultureName == TEXT("LEET"))
		{
			for(auto NamespaceIterator = LiveTable.NamespaceTable.CreateIterator(); NamespaceIterator; ++NamespaceIterator)
			{
				const FString& Namespace = NamespaceIterator.Key();
				FTextLookupTable::FKeyTable& LiveKeyTable = NamespaceIterator.Value();
				for(auto KeyIterator = LiveKeyTable.CreateIterator(); KeyIterator; ++KeyIterator)
				{
					const FString& Key = KeyIterator.Key();
					FStringEntry& LiveStringEntry = KeyIterator.Value();
					LiveStringEntry.bIsLocalized = true;
					FInternationalization::Leetify( *LiveStringEntry.String );
				}
			}

			return;
		}
	}
#endif

	TArray<FString> LocalizationPaths;
	if(ShouldLoadGame)
	{
		LocalizationPaths += FPaths::GetGameLocalizationPaths();
	}
	if(ShouldLoadEditor)
	{
		LocalizationPaths += FPaths::GetEditorLocalizationPaths();
		LocalizationPaths += FPaths::GetToolTipLocalizationPaths();

		bool bShouldLoadLocalizedPropertyNames = true;
		if( !GConfig->GetBool( TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), bShouldLoadLocalizedPropertyNames, GEditorGameAgnosticIni ) )
		{
			GConfig->GetBool( TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), bShouldLoadLocalizedPropertyNames, GEngineIni );
		}
		if(bShouldLoadLocalizedPropertyNames)
		{
			LocalizationPaths += FPaths::GetPropertyNameLocalizationPaths();
		}
	}
	LocalizationPaths += FPaths::GetEngineLocalizationPaths();

	// Prioritized array of localization entry trackers.
	TArray<FLocalizationEntryTracker> LocalizationEntryTrackers;

	const auto MapCulturesToDirectories = [](const FString& LocalizationPath) -> TMap<FString, FString>
	{
		TMap<FString, FString> CultureToDirectoryMap;
		IFileManager& FileManager = IFileManager::Get();

		/* Visitor class used to enumerate directories of culture */
		class FCultureDirectoryMapperVistor : public IPlatformFile::FDirectoryVisitor
		{
		public:
			FCultureDirectoryMapperVistor( TMap<FString, FString>& OutCultureToDirectoryMap )
				: CultureToDirectoryMap(OutCultureToDirectoryMap)
			{
			}

			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
			{
				if(bIsDirectory)
				{
					// UE localization resource folders use "en-US" style while ICU uses "en_US"
					const FString LocalizationFolder = FPaths::GetCleanFilename(FilenameOrDirectory);
					const FString CanonicalName = FCulture::GetCanonicalName(LocalizationFolder);
					CultureToDirectoryMap.Add(CanonicalName, LocalizationFolder);
				}

				return true;
			}

			/** Array to fill with the names of the UE localization folders available at the given path */
			TMap<FString, FString>& CultureToDirectoryMap;
		};

		FCultureDirectoryMapperVistor CultureEnumeratorVistor(CultureToDirectoryMap);
		FileManager.IterateDirectory(*LocalizationPath, CultureEnumeratorVistor);

		return CultureToDirectoryMap;
	};

	TMap< FString, TMap<FString, FString> > LocalizationPathToCultureDirectoryMap;
	for (const FString& LocalizationPath : LocalizationPaths)
	{
		LocalizationPathToCultureDirectoryMap.Add(LocalizationPath, MapCulturesToDirectories(LocalizationPath));
	}

	// Read culture localization resources.
	TArray<FString> PrioritizedParentCultureNames = I18N.GetCurrentCulture()->GetPrioritizedParentCultureNames();

	for (const FString& CultureName : PrioritizedParentCultureNames)
	{
		FLocalizationEntryTracker& CultureTracker = LocalizationEntryTrackers[LocalizationEntryTrackers.Add(FLocalizationEntryTracker())];
		for (const FString& LocalizationPath : LocalizationPaths)
		{
			const FString* const Entry = LocalizationPathToCultureDirectoryMap[LocalizationPath].Find(FCulture::GetCanonicalName(CultureName));
			if (Entry)
			{
				const FString CulturePath = LocalizationPath / (*Entry);

				CultureTracker.ReadFromDirectory(CulturePath);
			}
		}
		CultureTracker.ReportCollisions();
	}

	UpdateLiveTable(LocalizationEntryTrackers);
}

void FTextLocalizationManager::UpdateLiveTable(const TArray<FLocalizationEntryTracker>& LocalizationEntryTrackers, const bool FilterUpdatesByTableName)
{
	// Update existing localized entries/flag existing newly-unlocalized entries.
	for(auto NamespaceIterator = LiveTable.NamespaceTable.CreateIterator(); NamespaceIterator; ++NamespaceIterator)
	{
		const FString& NamespaceName = NamespaceIterator.Key();
		FTextLookupTable::FKeyTable& LiveKeyTable = NamespaceIterator.Value();
		for(auto KeyIterator = LiveKeyTable.CreateIterator(); KeyIterator; ++KeyIterator)
		{
			const FString& KeyName = KeyIterator.Key();
			FStringEntry& LiveStringEntry = KeyIterator.Value();

			const FLocalizationEntryTracker::FEntry* UpdateEntry = NULL;

			// Attempt to use resources in prioritized order until we find an entry.
			for(int32 i = 0; i < LocalizationEntryTrackers.Num() && !UpdateEntry; ++i)
			{
				const FLocalizationEntryTracker& Tracker = LocalizationEntryTrackers[i];
				const FLocalizationEntryTracker::FKeyTable* const UpdateKeyTable = Tracker.Namespaces.Find(NamespaceName);
				const FLocalizationEntryTracker::FEntryArray* const UpdateEntryArray = UpdateKeyTable ? UpdateKeyTable->Find(KeyName) : NULL;
				const FLocalizationEntryTracker::FEntry* Entry = UpdateEntryArray && UpdateEntryArray->Num() ? &((*UpdateEntryArray)[0]) : NULL;
				UpdateEntry = (Entry && (!FilterUpdatesByTableName || LiveStringEntry.TableName == Entry->TableName)) ? Entry : NULL;
			}

			if( UpdateEntry )
			{
				// If an entry is unlocalized and the source hash differs, it suggests that the source hash changed - do not replace the display string.
				if(LiveStringEntry.bIsLocalized || LiveStringEntry.SourceStringHash == UpdateEntry->SourceStringHash)
				{
					LiveStringEntry.bIsLocalized = true;
					*(LiveStringEntry.String) = UpdateEntry->LocalizedString;
					LiveStringEntry.SourceStringHash = UpdateEntry->SourceStringHash;
				}
			}
			else if(!FilterUpdatesByTableName)
			{
				if ( !LiveStringEntry.bIsLocalized && LiveStringEntry.String.Get() == AccessedStringBeforeLocLoadedErrorMsg )
				{
					*(LiveStringEntry.String) = TEXT("");
				}

				LiveStringEntry.bIsLocalized = false;

#if ENABLE_LOC_TESTING
				const bool bShouldLEETIFYUnlocalizedString = FParse::Param(FCommandLine::Get(), TEXT("LEETIFYUnlocalized"));
				if(bShouldLEETIFYUnlocalizedString )
				{
					FInternationalization::Leetify(*(LiveStringEntry.String));
				}
#endif
			}
		}
	}

	// Add new entries. 
	for(int32 i = 0; i < LocalizationEntryTrackers.Num(); ++i)
	{
		const FLocalizationEntryTracker& Tracker = LocalizationEntryTrackers[i];
		for(auto NamespaceIterator = Tracker.Namespaces.CreateConstIterator(); NamespaceIterator; ++NamespaceIterator)
		{
			const FString& NamespaceName = NamespaceIterator.Key();
			const FLocalizationEntryTracker::FKeyTable& NewKeyTable = NamespaceIterator.Value();
			for(auto KeyIterator = NewKeyTable.CreateConstIterator(); KeyIterator; ++KeyIterator)
			{
				const FString& Key = KeyIterator.Key();
				const FLocalizationEntryTracker::FEntryArray& NewEntryArray = KeyIterator.Value();
				const FLocalizationEntryTracker::FEntry& NewEntry = NewEntryArray[0];

				FTextLookupTable::FKeyTable& LiveKeyTable = LiveTable.NamespaceTable.FindOrAdd(NamespaceName);
				FStringEntry* const LiveStringEntry = LiveKeyTable.Find(Key);
				// Note: Anything we find in the table has already been updated above.
				if( !LiveStringEntry )
				{
					FStringEntry NewLiveEntry(
						true,													/*bIsLocalized*/
						NewEntry.TableName,
						NewEntry.SourceStringHash,								/*SourceStringHash*/
						MakeShareable( new FString(NewEntry.LocalizedString) )	/*String*/
						);
					LiveKeyTable.Add( Key, NewLiveEntry );

					ReverseLiveTable.Add(NewLiveEntry.String, FNamespaceKeyEntry( NamespaceName.IsEmpty()? nullptr : MakeShareable( new FString( NamespaceName ) ), Key.IsEmpty()? nullptr : MakeShareable( new FString( Key ) )));
				}
			}
		}
	}

	OnTranslationsChanged().Broadcast();
}

void FTextLocalizationManager::OnCultureChanged()
{
	const bool ShouldLoadEditor = bIsInitialized && WITH_EDITOR;
	const bool ShouldLoadGame = bIsInitialized && FApp::IsGame();
	LoadResources(ShouldLoadEditor, ShouldLoadGame);
}

TSharedPtr<FString, ESPMode::ThreadSafe> FTextLocalizationManager::FindString( const FString& Namespace, const FString& Key, const FString* const SourceString )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	// Find namespace's key table.
	const FTextLookupTable::FKeyTable* LiveKeyTable = LiveTable.NamespaceTable.Find( Namespace );

	// Find key table's entry.
	const FStringEntry* LiveEntry = LiveKeyTable ? LiveKeyTable->Find( Key ) : NULL;

	if( LiveEntry != NULL && ( !SourceString || LiveEntry->SourceStringHash == FCrc::StrCrc32(**SourceString) ) )
	{
		return LiveEntry->String;
	}

	return NULL;
}

TSharedPtr<FString, ESPMode::ThreadSafe> FTextLocalizationManager::GetTableName(const FString& Namespace, const FString& Key)
{
	FScopeLock ScopeLock(&SynchronizationObject);

	// Find namespace's key table.
	const FTextLookupTable::FKeyTable* LiveKeyTable = LiveTable.NamespaceTable.Find(Namespace);

	// Find key table's entry.
	const FStringEntry* LiveEntry = LiveKeyTable ? LiveKeyTable->Find(Key) : NULL;

	if (LiveEntry != nullptr)
	{
		return MakeShareable(new FString((LiveEntry->TableName)));
	}

	return NULL;
}


TSharedRef<FString, ESPMode::ThreadSafe> FTextLocalizationManager::GetString(const FString& Namespace, const FString& Key, const FString* const SourceString)
{
	FScopeLock ScopeLock( &SynchronizationObject );

	// Hack fix for old assets that don't have namespace/key info
	if(Namespace.IsEmpty() && Key.IsEmpty())
	{
		return MakeShareable( new FString( SourceString ? **SourceString : TEXT("") ) );
	}

#if ENABLE_LOC_TESTING
	const bool bShouldLEETIFYAll = bIsInitialized && FInternationalization::Get().GetCurrentCulture()->GetName() == TEXT("LEET");

	// Attempt to set bShouldLEETIFYUnlocalizedString appropriately, only once, after the commandline is initialized and parsed
	static bool bShouldLEETIFYUnlocalizedString = false;
	{
		static bool bHasParsedCommandLine = false;
		if(!bHasParsedCommandLine && FCommandLine::IsInitialized())
		{
			bShouldLEETIFYUnlocalizedString = FParse::Param(FCommandLine::Get(), TEXT("LEETIFYUnlocalized"));
			bHasParsedCommandLine = true;
		}
	}
#endif

	// Find namespace's key table.
	FTextLookupTable::FKeyTable* LiveKeyTable = LiveTable.NamespaceTable.Find( Namespace );

	// Find key table's entry.
	FStringEntry* LiveEntry = LiveKeyTable ? LiveKeyTable->Find( Key ) : NULL;

	// Entry is present.
	if( LiveEntry )
	{
		// If we're in some sort of development setting, source may have changed but localization resources have not, in which case the source should be used.
		const uint32 SourceStringHash = SourceString ? FCrc::StrCrc32(**SourceString) : 0;

		// If the source string (hash) is different, the local source has changed and should override - can't be localized.
		if( SourceStringHash != 0 && SourceStringHash != LiveEntry->SourceStringHash )
		{
			LiveEntry->SourceStringHash = SourceStringHash;
			LiveEntry->String.Get() = SourceString ? **SourceString : TEXT("");

#if ENABLE_LOC_TESTING
			if( (bShouldLEETIFYAll || bShouldLEETIFYUnlocalizedString) && SourceString )
			{
				FInternationalization::Leetify(*LiveEntry->String);
				if(*LiveEntry->String == *SourceString)
				{
					UE_LOG(LogTextLocalizationManager, Warning, TEXT("Leetify failed to alter a string (%s)."), **SourceString );
				}
			}
#endif

			UE_LOG(LogTextLocalizationManager, Verbose, TEXT("An attempt was made to get a localized string (Namespace:%s, Key:%s), but the source string hash does not match - the source string (%s) will be used."), *Namespace, *Key, **LiveEntry->String);

#if ENABLE_LOC_TESTING
			LiveEntry->bIsLocalized = bShouldLEETIFYAll;
#else
			LiveEntry->bIsLocalized = false;
#endif
		}

		return LiveEntry->String;
	}
	// Entry is absent.
	else
	{
		// Don't log warnings about unlocalized strings if the system hasn't been initialized - we simply don't have localization data yet.
		if( bIsInitialized )
		{
			UE_LOG(LogTextLocalizationManager, Verbose, TEXT("An attempt was made to get a localized string (Namespace:%s, Key:%s, Source:%s), but it did not exist."), *Namespace, *Key, SourceString ? **SourceString : TEXT(""));
		}

		const TSharedRef<FString, ESPMode::ThreadSafe> UnlocalizedString = MakeShareable( new FString( SourceString ? **SourceString : TEXT("") ) );

		// If live-culture-swap is enabled or the system is uninitialized - make entries so that they can be updated when system is initialized or a culture swap occurs.
		CA_SUPPRESS(6236)
		CA_SUPPRESS(6316)
		if( !(bIsInitialized) || ENABLE_LOC_TESTING )
		{
#if ENABLE_LOC_TESTING
			if( (bShouldLEETIFYAll || bShouldLEETIFYUnlocalizedString) && SourceString )
			{
				FInternationalization::Leetify(*UnlocalizedString);
				if(*UnlocalizedString == *SourceString)
				{
					UE_LOG(LogTextLocalizationManager, Warning, TEXT("Leetify failed to alter a string (%s)."), **SourceString );
				}
			}
#endif

			if ( UnlocalizedString->IsEmpty() )
			{
				if ( !bIsInitialized )
				{
					*(UnlocalizedString) = AccessedStringBeforeLocLoadedErrorMsg;
				}
			}

			FStringEntry NewEntry(
#if ENABLE_LOC_TESTING
				bShouldLEETIFYAll						/*bIsLocalized*/
#else
				false												/*bIsLocalized*/
#endif
				, TEXT("")
				, SourceString ? FCrc::StrCrc32(**SourceString) : 0	/*SourceStringHash*/
				, UnlocalizedString									/*String*/
				);

			if( !LiveKeyTable )
			{
				LiveKeyTable = &(LiveTable.NamespaceTable.Add( Namespace, FTextLookupTable::FKeyTable() ));
			}

			LiveKeyTable->Add( Key, NewEntry );

			ReverseLiveTable.Add(NewEntry.String, FNamespaceKeyEntry( Namespace.IsEmpty()? nullptr : MakeShareable( new FString( Namespace ) ), Key.IsEmpty()? nullptr : MakeShareable( new FString( Key ) )));

		}

		return UnlocalizedString;
	}
}

void FTextLocalizationManager::FindKeyNamespaceFromDisplayString(TSharedRef<FString, ESPMode::ThreadSafe> InDisplayString, TSharedPtr<FString, ESPMode::ThreadSafe>& OutNamespace, TSharedPtr<FString, ESPMode::ThreadSafe>& OutKey)
{
	FScopeLock ScopeLock( &SynchronizationObject );

	FNamespaceKeyEntry* NamespaceKeyEntry = ReverseLiveTable.Find(InDisplayString);

	if(NamespaceKeyEntry)
	{
		OutNamespace = NamespaceKeyEntry->Namespace;
		OutKey = NamespaceKeyEntry->Key;
	}
}


void FTextLocalizationManager::RegenerateResources( const FString& ConfigFilePath, IInternationalizationArchiveSerializer& ArchiveSerializer, IInternationalizationManifestSerializer& ManifestSerializer )
{
	// Add one to the revision index, so all FText's refresh.
	++HeadCultureRevisionIndex;
	
	FInternationalization& I18N = FInternationalization::Get();

	FString SectionName = TEXT("RegenerateResources");

	// Get source path.
	FString SourcePath;
	if( !( GConfig->GetString( *SectionName, TEXT("SourcePath"), SourcePath, ConfigFilePath ) ) )
	{
		UE_LOG(LogTextLocalizationManager, Error, TEXT("No source path specified."));
		return;
	}

	// Get destination path.
	FString DestinationPath;
	if( !( GConfig->GetString( *SectionName, TEXT("DestinationPath"), DestinationPath, ConfigFilePath ) ) )
	{
		UE_LOG(LogTextLocalizationManager, Error, TEXT("No destination path specified."));
		return;
	}

	// Get manifest name.
	FString ManifestName;
	if( !( GConfig->GetString( *SectionName, TEXT("ManifestName"), ManifestName, ConfigFilePath ) ) )
	{
		UE_LOG(LogTextLocalizationManager, Error, TEXT("No manifest name specified."));
		return;
	}

	// Get resource name.
	FString ResourceName;
	if( !( GConfig->GetString( *SectionName, TEXT("ResourceName"), ResourceName, ConfigFilePath ) ) )
	{
		UE_LOG(LogTextLocalizationManager, Error, TEXT("No resource name specified."));
		return;
	}

	TArray<FString> LocaleNames;
	{
		const FString CultureName = I18N.GetCurrentCulture()->GetName();
		LocaleNames.Add(CultureName);
		const FString BaseLanguageName = I18N.GetCurrentCulture()->GetTwoLetterISOLanguageName();
		if(BaseLanguageName != CultureName)
		{
			LocaleNames.Add(BaseLanguageName);
		}
	}

	// Source path needs to be relative to Engine or Game directory
	FString ConfigFullPath = FPaths::ConvertRelativePathToFull(ConfigFilePath);
	FString EngineFullPath = FPaths::ConvertRelativePathToFull(FPaths::EngineConfigDir());
	bool IsEngineManifest = false;
	
	if (ConfigFullPath.StartsWith(EngineFullPath))
	{
		IsEngineManifest = true;
	}

	if (IsEngineManifest)
	{
		SourcePath = FPaths::Combine(*(FPaths::EngineDir()), *SourcePath);
		DestinationPath = FPaths::Combine(*(FPaths::EngineDir()), *DestinationPath);
	}
	else
	{
		SourcePath = FPaths::Combine(*(FPaths::GameDir()), *SourcePath);
		DestinationPath = FPaths::Combine(*(FPaths::GameDir()), *DestinationPath);
	}

	TArray<TArray<uint8>> BackingBuffers;
	BackingBuffers.SetNum(LocaleNames.Num());

	for(int32 i = 0; i < BackingBuffers.Num(); ++i)
	{
		TArray<uint8>& BackingBuffer = BackingBuffers[i];
		FMemoryWriter MemoryWriter(BackingBuffer, true);

		// Read the manifest file from the source path.
		FString ManifestFilePath = (SourcePath / ManifestName);
		ManifestFilePath = FPaths::ConvertRelativePathToFull(ManifestFilePath);
		TSharedRef<FInternationalizationManifest> InternationalizationManifest = MakeShareable(new FInternationalizationManifest);

#if 0 // @todo Json: Serializing from FArchive is currently broken
		FArchive* ManifestFile = IFileManager::Get().CreateFileReader(*ManifestFilePath);

		if (ManifestFile == nullptr)
		{
			UE_LOG(LogTextLocalizationManager, Error, TEXT("No manifest found at %s."), *ManifestFilePath);
			return;
		}

		ManifestSerializer.DeserializeManifest(*ManifestFile, InternationalizationManifest);
#else
		FString ManifestContent;

		if (!FFileHelper::LoadFileToString(ManifestContent, *ManifestFilePath))
		{
			UE_LOG(LogTextLocalizationManager, Error, TEXT("Failed to load file %s."), *ManifestFilePath);
			continue;
		}

		ManifestSerializer.DeserializeManifest(ManifestContent, InternationalizationManifest);
#endif

		// Write resource.
		FTextLocalizationResourceGenerator::Generate(SourcePath, InternationalizationManifest, LocaleNames[i], &(MemoryWriter), ArchiveSerializer);

		MemoryWriter.Close();
	}

	// Prioritized array of localization entry trackers.
	TArray<FLocalizationEntryTracker> LocalizationEntryTrackers;
	for(int32 i = 0; i < BackingBuffers.Num(); ++i)
	{
		TArray<uint8>& BackingBuffer = BackingBuffers[i];
		FMemoryReader MemoryReader(BackingBuffer, true);
		const FString CulturePath = DestinationPath / LocaleNames[i];
		const FString ResourceFilePath = FPaths::ConvertRelativePathToFull(CulturePath / ResourceName);

		FLocalizationEntryTracker& CultureTracker = LocalizationEntryTrackers[LocalizationEntryTrackers.Add(FLocalizationEntryTracker())];
		CultureTracker.ReadFromArchive(MemoryReader, ResourceFilePath);
		CultureTracker.ReportCollisions();

		MemoryReader.Close();
	}

	// Don't filter updates by table name, or we can't live preview strings that had no translation originally
	UpdateLiveTable(LocalizationEntryTrackers);
}
