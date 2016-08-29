// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "InternationalizationPrivatePCH.h"
#include "TextLocalizationResourceGenerator.h"


DEFINE_LOG_CATEGORY_STATIC(LogTextLocalizationResourceGenerator, Log, All);


void FTextLocalizationResourceGenerator::FLocalizationEntryTracker::ReportCollisions() const
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
					const bool bDoesLocalizedStringDiffer = !LeftEntry.LocalizedString.Equals( RightEntry.LocalizedString, ESearchCase::CaseSensitive );
					bWasCollisionDetected = bDoesLocalizedStringDiffer;
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

					CollidingEntryListString += FString::Printf( TEXT("String: (%s)"), *Entry.LocalizedString );
				}

				UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("Archives contain conflicting entries for (Namespace:%s, Key:%s):\n%s"), *NamespaceName, *KeyName, *CollidingEntryListString);
			}
		}
	}
}

bool FTextLocalizationResourceGenerator::FLocalizationEntryTracker::WriteToArchive(FArchive* const Archive)
{
	bool WasSuccessful = true;

	FLocalizationEntryTracker& NonConstThis = const_cast<FLocalizationEntryTracker&>(*this); // Necessary conversion because FArchive lacks const-correctness using the insertion operator.

	if ( Archive == NULL )
	{
		return false;
	}

	Archive->SetForceUnicode(true);

	// Write namespace count
	uint32 NamespaceCount = NonConstThis.Namespaces.Num();
	*Archive << NamespaceCount;

	// Iterate through namespaces
	for(auto NamespaceIterator = NonConstThis.Namespaces.CreateIterator(); NamespaceIterator; ++NamespaceIterator)
	{
		/*const*/ FString Namespace = NamespaceIterator.Key();
		/*const*/ FLocalizationEntryTracker::FKeyTable& KeyTable = NamespaceIterator.Value();

		// Write namespace.
		*Archive << Namespace;

		// Write key count.
		const int32 KeyCountOffsetInFile = Archive->Tell();
		const uint32 OriginalKeyCount = KeyTable.Num();
		uint32 KeyCount = OriginalKeyCount;
		*Archive << KeyCount;

		// Iterate through keys and values
		for(auto KeyIterator = KeyTable.CreateIterator(); KeyIterator; ++KeyIterator)
		{
			/*const*/ FString Key = KeyIterator.Key();
			/*const*/ FLocalizationEntryTracker::FEntryArray& EntryArray = KeyIterator.Value();

			// Skip this key if there are no entries.
			if(EntryArray.Num() == 0)
			{
				UE_LOG(LogTextLocalizationResourceGenerator, Warning, TEXT("Archives contained no entries for key (%s)"), *(Key));
				--KeyCount; // We've skipped an entry and thus a key and the key count must be adjusted.
				continue;
			}

			// Find first valid entry.
			/*const*/ FLocalizationEntryTracker::FEntry* Value = NULL;
			for(int32 i = 0; i < EntryArray.Num(); ++i)
			{
				if( !( EntryArray[i].LocalizedString.IsEmpty() ) )
				{
					Value = &(EntryArray[i]);
					break;
				}
			}

			// Skip this key if there is no valid entry.
			if( !(Value) )
			{
				UE_LOG(LogTextLocalizationResourceGenerator, Verbose, TEXT("Archives contained only blank entries for key (%s)"), *(Key));
				--KeyCount; // We've skipped an entry and thus a key and the key count must be adjusted.
				continue;
			}

			// Write key.
			*Archive << Key;

			// Write string entry.
			*Archive << Value->SourceStringHash;

			*Archive << Value->LocalizedString;
		}

		// If key count is different than original key count (due to skipped entries), adjust key count in file.
		if(KeyCount != OriginalKeyCount)
		{
			const int32 LatestOffsetInFile = Archive->Tell();
			Archive->Seek(KeyCountOffsetInFile);
			*Archive << KeyCount;
			Archive->Seek(LatestOffsetInFile);
		}
	}

	return WasSuccessful;
}

bool FTextLocalizationResourceGenerator::Generate(const FLocTextHelper& InLocTextHelper, const FString& InCultureToGenerate, const bool bSkipSourceCheck, FArchive& InDestinationArchive)
{
	FLocalizationEntryTracker LocalizationEntryTracker;

	// Add each manifest entry to the LocRes file
	InLocTextHelper.EnumerateSourceTexts([&InLocTextHelper, &InCultureToGenerate, &LocalizationEntryTracker, &bSkipSourceCheck](TSharedRef<FManifestEntry> InManifestEntry) -> bool
	{
		// For each context, we may need to create a different or even multiple LocRes entries.
		for (const FManifestContext& Context : InManifestEntry->Contexts)
		{
			// Find the correct translation based upon the native source text
			FLocItem TranslationText;
			InLocTextHelper.GetRuntimeText(InCultureToGenerate, InManifestEntry->Namespace, Context.Key, Context.KeyMetadataObj, ELocTextExportSourceMethod::NativeText, InManifestEntry->Source, TranslationText, bSkipSourceCheck);

			// Add this entry to the LocRes
			FLocalizationEntryTracker::FKeyTable& KeyTable = LocalizationEntryTracker.Namespaces.FindOrAdd(*InManifestEntry->Namespace);
			FLocalizationEntryTracker::FEntryArray& EntryArray = KeyTable.FindOrAdd(*Context.Key);
			FLocalizationEntryTracker::FEntry& NewEntry = EntryArray[EntryArray.AddDefaulted()];
			NewEntry.SourceStringHash = FCrc::StrCrc32(*InManifestEntry->Source.Text);
			NewEntry.LocalizedString = TranslationText.Text;
		}

		return true; // continue enumeration
	}, true);

	LocalizationEntryTracker.ReportCollisions();

	// Write resource.
	if (!LocalizationEntryTracker.WriteToArchive(&InDestinationArchive))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("Failed to write localization entries to archive."));
		return false;
	}

	return true;
}

bool FTextLocalizationResourceGenerator::GenerateAndUpdateLiveEntriesFromConfig(const FString& InConfigFilePath, const bool bSkipSourceCheck)
{
	FInternationalization& I18N = FInternationalization::Get();

	const FString SectionName = TEXT("RegenerateResources");

	// Get native culture.
	FString NativeCulture;
	if (!GConfig->GetString(*SectionName, TEXT("NativeCulture"), NativeCulture, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No native culture specified."));
		return false;
	}

	// Get source path.
	FString SourcePath;
	if (!GConfig->GetString(*SectionName, TEXT("SourcePath"), SourcePath, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No source path specified."));
		return false;
	}

	// Get destination path.
	FString DestinationPath;
	if (!GConfig->GetString(*SectionName, TEXT("DestinationPath"), DestinationPath, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No destination path specified."));
		return false;
	}

	// Get manifest name.
	FString ManifestName;
	if (!GConfig->GetString(*SectionName, TEXT("ManifestName"), ManifestName, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No manifest name specified."));
		return false;
	}

	// Get manifest name.
	FString ArchiveName;
	if (!GConfig->GetString(*SectionName, TEXT("ArchiveName"), ManifestName, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No archive name specified."));
		return false;
	}

	// Get resource name.
	FString ResourceName;
	if (!GConfig->GetString(*SectionName, TEXT("ResourceName"), ResourceName, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No resource name specified."));
		return false;
	}

	// Source path needs to be relative to Engine or Game directory
	const FString ConfigFullPath = FPaths::ConvertRelativePathToFull(InConfigFilePath);
	const FString EngineFullPath = FPaths::ConvertRelativePathToFull(FPaths::EngineConfigDir());
	const bool IsEngineManifest = ConfigFullPath.StartsWith(EngineFullPath);

	if (IsEngineManifest)
	{
		SourcePath = FPaths::Combine(*FPaths::EngineDir(), *SourcePath);
		DestinationPath = FPaths::Combine(*FPaths::EngineDir(), *DestinationPath);
	}
	else
	{
		SourcePath = FPaths::Combine(*FPaths::GameDir(), *SourcePath);
		DestinationPath = FPaths::Combine(*FPaths::GameDir(), *DestinationPath);
	}

	TArray<FString> CulturesToGenerate;
	{
		const FString CultureName = I18N.GetCurrentCulture()->GetName();
		const TArray<FString> PrioritizedCultures = I18N.GetPrioritizedCultureNames(CultureName);
		for (const FString& PrioritizedCulture : PrioritizedCultures)
		{
			if (FPaths::FileExists(SourcePath / PrioritizedCulture / ArchiveName))
			{
				CulturesToGenerate.Add(PrioritizedCulture);
			}
		}
	}

	if (CulturesToGenerate.Num() == 0)
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No cultures to generate were specified."));
		return false;
	}

	// Load the manifest and all archives
	FLocTextHelper LocTextHelper(SourcePath, ManifestName, ArchiveName, NativeCulture, CulturesToGenerate, nullptr);
	{
		FText LoadError;
		if (!LocTextHelper.LoadAll(ELocTextHelperLoadFlags::LoadOrCreate, &LoadError))
		{
			UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("%s"), *LoadError.ToString());
			return false;
		}
	}

	TArray<TArray<uint8>> BackingBuffers;
	BackingBuffers.SetNum(CulturesToGenerate.Num());

	for (int32 CultureIndex = 0; CultureIndex < CulturesToGenerate.Num(); ++CultureIndex)
	{
		const FString& CultureName = CulturesToGenerate[CultureIndex];

		TArray<uint8>& BackingBuffer = BackingBuffers[CultureIndex];
		FMemoryWriter MemoryWriter(BackingBuffer, true);
		const bool bDidGenerate = Generate(LocTextHelper, CultureName, bSkipSourceCheck, MemoryWriter);
		MemoryWriter.Close();

		if (!bDidGenerate)
		{
			UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("Failed to generate localization resource for culture '%s'."), *CultureName);
			return false;
		}
	}

	for (int32 CultureIndex = 0; CultureIndex < CulturesToGenerate.Num(); ++CultureIndex)
	{
		const FString& CultureName = CulturesToGenerate[CultureIndex];
		const FString CulturePath = DestinationPath / CultureName;
		const FString ResourceFilePath = FPaths::ConvertRelativePathToFull(CulturePath / ResourceName);

		TArray<uint8>& BackingBuffer = BackingBuffers[CultureIndex];
		FMemoryReader MemoryReader(BackingBuffer, true);
		FTextLocalizationManager::Get().UpdateFromLocalizationResource(MemoryReader, ResourceFilePath);
		MemoryReader.Close();
	}

	return true;
}
