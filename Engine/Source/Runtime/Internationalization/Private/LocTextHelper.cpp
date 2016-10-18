// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "InternationalizationPrivatePCH.h"
#include "LocTextHelper.h"

#include "Json.h"
#include "JsonInternationalizationManifestSerializer.h"
#include "JsonInternationalizationArchiveSerializer.h"
#include "JsonInternationalizationMetadataSerializer.h"

#define LOCTEXT_NAMESPACE "LocTextHelper"

DEFINE_LOG_CATEGORY_STATIC(LogLocTextHelper, Log, All);

void FLocTextConflicts::AddConflict(const FString& InNamespace, const FString& InKey, const TSharedPtr<FLocMetadataObject>& InKeyMetadata, const FLocItem& InSource, const FString& InSourceLocation)
{
	TSharedPtr<FConflict> ExistingEntry = FindEntryByKey(InNamespace, InKey, InKeyMetadata);
	if (!ExistingEntry.IsValid())
	{
		TSharedRef<FConflict> NewEntry = MakeShareable(new FConflict(InNamespace, InKey, InKeyMetadata));
		EntriesByKey.Add(InKey, NewEntry);
		ExistingEntry = NewEntry;
	}
	ExistingEntry->Add(InSource, InSourceLocation.ReplaceCharWithEscapedChar());
}

TSharedPtr<FLocTextConflicts::FConflict> FLocTextConflicts::FindEntryByKey(const FString& InNamespace, const FString& InKey, const TSharedPtr<FLocMetadataObject> InKeyMetadata) const
{
	TArray<TSharedRef<FConflict>> MatchingEntries;
	EntriesByKey.MultiFind(InKey, MatchingEntries);

	for (const TSharedRef<FConflict>& Entry : MatchingEntries)
	{
		if (Entry->Namespace.Equals(InNamespace, ESearchCase::CaseSensitive))
		{
			if (InKeyMetadata.IsValid() != Entry->KeyMetadataObj.IsValid())
			{
				continue;
			}
			else if ((!InKeyMetadata.IsValid() && !Entry->KeyMetadataObj.IsValid()) || (*InKeyMetadata == *Entry->KeyMetadataObj))
			{
				return Entry;
			}
		}
	}

	return nullptr;
}

FString FLocTextConflicts::GetConflictReport() const
{
	FString Report;

	for (const auto& ConflictPair : EntriesByKey)
	{
		const TSharedRef<FConflict>& Conflict = ConflictPair.Value;
		const FString& Namespace = Conflict->Namespace;
		const FString& Key = Conflict->Key;

		bool bAddToReport = false;
		TArray<FLocItem> SourceList;
		Conflict->EntriesBySourceLocation.GenerateValueArray(SourceList);
		if (SourceList.Num() >= 2)
		{
			for (int32 i = 0; i < SourceList.Num() - 1 && !bAddToReport; ++i)
			{
				for (int32 j = i + 1; j < SourceList.Num() && !bAddToReport; ++j)
				{
					if (!(SourceList[i] == SourceList[j]))
					{
						bAddToReport = true;
					}
				}
			}
		}

		if (bAddToReport)
		{
			FString KeyMetadataString = FJsonInternationalizationMetaDataSerializer::MetadataToString(Conflict->KeyMetadataObj);
			Report += FString::Printf(TEXT("%s - %s %s\n"), *Namespace, *Key, *KeyMetadataString);

			for (auto EntryIter = Conflict->EntriesBySourceLocation.CreateConstIterator(); EntryIter; ++EntryIter)
			{
				const FString& SourceLocation = EntryIter.Key();
				FString ProcessedSourceLocation = FPaths::ConvertRelativePathToFull(SourceLocation);
				ProcessedSourceLocation.ReplaceInline(TEXT("\\"), TEXT("/"));
				ProcessedSourceLocation.ReplaceInline(*FPaths::RootDir(), TEXT("/"));

				const FString& SourceText = EntryIter.Value().Text.ReplaceCharWithEscapedChar();

				FString SourceMetadataString = FJsonInternationalizationMetaDataSerializer::MetadataToString(EntryIter.Value().MetadataObj);
				Report += FString::Printf(TEXT("\t%s - \"%s\" %s\n"), *ProcessedSourceLocation, *SourceText, *SourceMetadataString);
			}
			Report += TEXT("\n");
		}
	}

	return Report;
}

FLocTextHelper::FLocTextHelper(TSharedPtr<ILocFileNotifies> InLocFileNotifies)
	: LocFileNotifies(MoveTemp(InLocFileNotifies))
{
}

FLocTextHelper::FLocTextHelper(FString InTargetPath, FString InManifestName, FString InArchiveName, FString InNativeCulture, TArray<FString> InForeignCultures, TSharedPtr<ILocFileNotifies> InLocFileNotifies)
	: TargetPath(MoveTemp(InTargetPath))
	, ManifestName(MoveTemp(InManifestName))
	, ArchiveName(MoveTemp(InArchiveName))
	, NativeCulture(MoveTemp(InNativeCulture))
	, ForeignCultures(MoveTemp(InForeignCultures))
	, LocFileNotifies(MoveTemp(InLocFileNotifies))
{
	checkf(!TargetPath.IsEmpty(), TEXT("Target path may not be empty!"));
	checkf(!ManifestName.IsEmpty(), TEXT("Manifest name may not be empty!"));
	checkf(!ArchiveName.IsEmpty(), TEXT("Archive name may not be empty!"));

	// Make sure the native culture isn't in the list of foreign cultures
	if (!NativeCulture.IsEmpty())
	{
		ForeignCultures.Remove(NativeCulture);
	}
}

bool FLocTextHelper::HasManifest() const
{
	return Manifest.IsValid();
}

bool FLocTextHelper::LoadManifest(const ELocTextHelperLoadFlags InLoadFlags, FText* OutError)
{
	const FString ManifestFilePath = TargetPath / ManifestName;
	return LoadManifest(ManifestFilePath, InLoadFlags, OutError);
}

bool FLocTextHelper::LoadManifest(const FString& InManifestFilePath, const ELocTextHelperLoadFlags InLoadFlags, FText* OutError)
{
	Manifest.Reset();
	Manifest = LoadManifestImpl(InManifestFilePath, InLoadFlags, OutError);
	return Manifest.IsValid();
}

bool FLocTextHelper::SaveManifest(FText* OutError) const
{
	const FString ManifestFilePath = TargetPath / ManifestName;
	return SaveManifest(ManifestFilePath, OutError);
}

bool FLocTextHelper::SaveManifest(const FString& InManifestFilePath, FText* OutError) const
{
	if (!Manifest.IsValid())
	{
		if (OutError)
		{
			*OutError = FText::Format(LOCTEXT("Error_SaveManifest_NoManifest", "Failed to save file '{0}' as there is no manifest instance to save."), FText::FromString(InManifestFilePath));
		}
		return false;
	}

	return SaveManifestImpl(Manifest.ToSharedRef(), InManifestFilePath, OutError);
}

void FLocTextHelper::TrimManifest()
{
	if (Dependencies.Num() > 0)
	{
		// We'll generate a new manifest by only including items that are not in the dependencies
		TSharedRef<FInternationalizationManifest> TrimmedManifest = MakeShareable(new FInternationalizationManifest());

		for (FManifestEntryByStringContainer::TConstIterator It(Manifest->GetEntriesByKeyIterator()); It; ++It)
		{
			const TSharedRef<FManifestEntry> ManifestEntry = It.Value();

			for (const FManifestContext& Context : ManifestEntry->Contexts)
			{
				FString DependencyFileName;
				const TSharedPtr<FManifestEntry> DependencyEntry = FindDependencyEntry(ManifestEntry->Namespace, Context, &DependencyFileName);

				if (DependencyEntry.IsValid())
				{
					if (!(DependencyEntry->Source.IsExactMatch(ManifestEntry->Source)))
					{
						// There is a dependency manifest entry that has the same namespace and keys as our main manifest entry but the source text differs.
						FString Message = SanitizeLogOutput(
							FString::Printf(TEXT("Found previously entered localized string [%s] %s %s=\"%s\" %s. It was previously \"%s\" %s in dependency manifest %s."),
								*ManifestEntry->Namespace,
								*Context.Key,
								*FJsonInternationalizationMetaDataSerializer::MetadataToString(Context.KeyMetadataObj),
								*ManifestEntry->Source.Text,
								*FJsonInternationalizationMetaDataSerializer::MetadataToString(ManifestEntry->Source.MetadataObj),
								*DependencyEntry->Source.Text,
								*FJsonInternationalizationMetaDataSerializer::MetadataToString(DependencyEntry->Source.MetadataObj),
								*DependencyFileName
								)
							);
						UE_LOG(LogLocTextHelper, Warning, TEXT("%s"), *Message);

						ConflictTracker.AddConflict(ManifestEntry->Namespace, Context.Key, Context.KeyMetadataObj, ManifestEntry->Source, *Context.SourceLocation);

						const FManifestContext* ConflictingContext = DependencyEntry->FindContext(Context.Key, Context.KeyMetadataObj);
						const FString DependencyEntryFullSrcLoc = (!DependencyFileName.IsEmpty()) ? DependencyFileName : ConflictingContext->SourceLocation;

						ConflictTracker.AddConflict(ManifestEntry->Namespace, Context.Key, Context.KeyMetadataObj, DependencyEntry->Source, DependencyEntryFullSrcLoc);
					}
				}
				else
				{
					// Since we did not find any entries in the dependencies list that match, we'll add to the new manifest
					const bool bAddSuccessful = TrimmedManifest->AddSource(ManifestEntry->Namespace, ManifestEntry->Source, Context);
					if (!bAddSuccessful)
					{
						UE_LOG(LogLocTextHelper, Error, TEXT("Could not process localized string: %s [%s] %s=\"%s\" %s."),
							*ManifestEntry->Namespace,
							*Context.Key,
							*ManifestEntry->Source.Text,
							*FJsonInternationalizationMetaDataSerializer::MetadataToString(ManifestEntry->Source.MetadataObj)
							);
					}
				}
			}
		}

		Manifest = TrimmedManifest;
	}
}

bool FLocTextHelper::HasNativeArchive() const
{
	TSharedPtr<FInternationalizationArchive> Archive = (NativeCulture.IsEmpty()) ? TSharedPtr<FInternationalizationArchive>() : Archives.FindRef(NativeCulture);
	return Archive.IsValid();
}

bool FLocTextHelper::LoadNativeArchive(const ELocTextHelperLoadFlags InLoadFlags, FText* OutError)
{
	const FString ArchiveFilePath = TargetPath / NativeCulture / ArchiveName;
	return LoadNativeArchive(ArchiveFilePath, InLoadFlags, OutError);
}

bool FLocTextHelper::LoadNativeArchive(const FString& InArchiveFilePath, const ELocTextHelperLoadFlags InLoadFlags, FText* OutError)
{
	checkf(!NativeCulture.IsEmpty(), TEXT("Attempted to load the native culture archive file, but no native culture was set during construction!"));
	checkf(Manifest.IsValid(), TEXT("Attempted to load the native culture archive file, but no manifest has been loaded!"));

	Archives.Remove(NativeCulture);
	
	TSharedPtr<FInternationalizationArchive> Archive = LoadArchiveImpl(InArchiveFilePath, InLoadFlags, OutError);
	if (Archive.IsValid())
	{
		Archives.Add(NativeCulture, Archive);
		return true;
	}
	return false;
}

bool FLocTextHelper::SaveNativeArchive(FText* OutError) const
{
	const FString ArchiveFilePath = TargetPath / NativeCulture / ArchiveName;
	return SaveNativeArchive(ArchiveFilePath, OutError);
}

bool FLocTextHelper::SaveNativeArchive(const FString& InArchiveFilePath, FText* OutError) const
{
	TSharedPtr<FInternationalizationArchive> Archive = (NativeCulture.IsEmpty()) ? TSharedPtr<FInternationalizationArchive>() : Archives.FindRef(NativeCulture);
	if (!Archive.IsValid())
	{
		if (OutError)
		{
			*OutError = FText::Format(LOCTEXT("Error_SaveArchive_NoArchive", "Failed to save file '{0}' as there is no archive instance to save."), FText::FromString(InArchiveFilePath));
		}
		return false;
	}

	return SaveArchiveImpl(Archive.ToSharedRef(), InArchiveFilePath, OutError);
}

bool FLocTextHelper::HasForeignArchive(const FString& InCulture) const
{
	TSharedPtr<FInternationalizationArchive> Archive = Archives.FindRef(InCulture);
	return Archive.IsValid();
}

bool FLocTextHelper::LoadForeignArchive(const FString& InCulture, const ELocTextHelperLoadFlags InLoadFlags, FText* OutError)
{
	const FString ArchiveFilePath = TargetPath / InCulture / ArchiveName;
	return LoadForeignArchive(InCulture, ArchiveFilePath, InLoadFlags, OutError);
}

bool FLocTextHelper::LoadForeignArchive(const FString& InCulture, const FString& InArchiveFilePath, const ELocTextHelperLoadFlags InLoadFlags, FText* OutError)
{
	checkf(ForeignCultures.Contains(InCulture), TEXT("Attempted to load a foreign culture archive file, but the given culture (%s) wasn't set during construction!"), *InCulture);
	checkf(Manifest.IsValid(), TEXT("Attempted to load a foreign culture archive file, but no manifest has been loaded!"));

	Archives.Remove(InCulture);

	TSharedPtr<FInternationalizationArchive> Archive = LoadArchiveImpl(InArchiveFilePath, InLoadFlags, OutError);
	if (Archive.IsValid())
	{
		Archives.Add(InCulture, Archive);
		return true;
	}
	return false;
}

bool FLocTextHelper::SaveForeignArchive(const FString& InCulture, FText* OutError) const
{
	const FString ArchiveFilePath = TargetPath / InCulture / ArchiveName;
	return SaveForeignArchive(InCulture, ArchiveFilePath, OutError);
}

bool FLocTextHelper::SaveForeignArchive(const FString& InCulture, const FString& InArchiveFilePath, FText* OutError) const
{
	TSharedPtr<FInternationalizationArchive> Archive = Archives.FindRef(InCulture);
	if (!Archive.IsValid())
	{
		if (OutError)
		{
			*OutError = FText::Format(LOCTEXT("Error_SaveArchive_NoArchive", "Failed to save file '{0}' as there is no archive instance to save."), FText::FromString(InArchiveFilePath));
		}
		return false;
	}

	return SaveArchiveImpl(Archive.ToSharedRef(), InArchiveFilePath, OutError);
}

bool FLocTextHelper::LoadAllArchives(const ELocTextHelperLoadFlags InLoadFlags, FText* OutError)
{
	if (!NativeCulture.IsEmpty() && !LoadNativeArchive(InLoadFlags, OutError))
	{
		return false;
	}

	for (const FString& Culture : ForeignCultures)
	{
		if (!LoadForeignArchive(Culture, InLoadFlags, OutError))
		{
			return false;
		}
	}

	return true;
}

bool FLocTextHelper::SaveAllArchives(FText* OutError) const
{
	if (!NativeCulture.IsEmpty() && !SaveNativeArchive(OutError))
	{
		return false;
	}

	for (const FString& Culture : ForeignCultures)
	{
		if (!SaveForeignArchive(Culture, OutError))
		{
			return false;
		}
	}

	return true;
}

void FLocTextHelper::TrimArchive(const FString& InCulture)
{
	checkf(Manifest.IsValid(), TEXT("Attempted to trim an archive file, but no manifest has been loaded!"));

	TSharedPtr<FInternationalizationArchive> Archive = Archives.FindRef(InCulture);
	checkf(Archive.IsValid(), TEXT("Attempted to trim an archive file, but no valid archive could be found for '%s'!"), *InCulture);

	TSharedPtr<FInternationalizationArchive> NativeArchive;
	if (!NativeCulture.IsEmpty() && InCulture != NativeCulture)
	{
		NativeArchive = Archives.FindRef(NativeCulture);
		checkf(NativeArchive.IsValid(), TEXT("Attempted to trim an archive file, but no valid archive could be found for '%s'!"), *NativeCulture);
	}

	// Copy any translations that match current manifest entries over into the trimmed archive
	TSharedRef<FInternationalizationArchive> TrimmedArchive = MakeShareable(new FInternationalizationArchive());
	EnumerateSourceTexts([&](TSharedRef<FManifestEntry> InManifestEntry) -> bool
	{
		for (const FManifestContext& Context : InManifestEntry->Contexts)
		{
			// Keep any translation for the source text
			TSharedPtr<FArchiveEntry> ArchiveEntry = Archive->FindEntryByKey(InManifestEntry->Namespace, Context.Key, Context.KeyMetadataObj);
			if (ArchiveEntry.IsValid())
			{
				TrimmedArchive->AddEntry(ArchiveEntry.ToSharedRef());
			}
		}

		return true; // continue enumeration
	}, true);

	Archives.Remove(InCulture);
	Archives.Add(InCulture, TrimmedArchive);
}

bool FLocTextHelper::LoadAll(const ELocTextHelperLoadFlags InLoadFlags, FText* OutError)
{
	if (!LoadManifest(InLoadFlags, OutError))
	{
		return false;
	}

	return LoadAllArchives(InLoadFlags, OutError);
}

bool FLocTextHelper::SaveAll(FText* OutError) const
{
	if (!SaveManifest(OutError))
	{
		return false;
	}

	return SaveAllArchives(OutError);
}

bool FLocTextHelper::AddDependency(const FString& InDependencyFilePath, FText* OutError)
{
	if (DependencyPaths.Contains(InDependencyFilePath))
	{
		return true;
	}

	TSharedPtr<FInternationalizationManifest> DepManifest = LoadManifestImpl(InDependencyFilePath, ELocTextHelperLoadFlags::Load, OutError);
	if (DepManifest.IsValid())
	{
		DependencyPaths.Add(InDependencyFilePath);
		Dependencies.Add(DepManifest);
		return true;
	}

	return false;
}

TSharedPtr<FManifestEntry> FLocTextHelper::FindDependencyEntry(const FString& InNamespace, const FString& InKey, const FString* InSourceText, FString* OutDependencyFilePath) const
{
	for (int32 DepIndex = 0; DepIndex < Dependencies.Num(); ++DepIndex)
	{
		TSharedPtr<FInternationalizationManifest> DepManifest = Dependencies[DepIndex];

		const TSharedPtr<FManifestEntry> DepEntry = DepManifest->FindEntryByKey(InNamespace, InKey, InSourceText);
		if (DepEntry.IsValid())
		{
			if (OutDependencyFilePath)
			{
				*OutDependencyFilePath = DependencyPaths[DepIndex];
			}
			return DepEntry;
		}
	}

	return nullptr;
}

TSharedPtr<FManifestEntry> FLocTextHelper::FindDependencyEntry(const FString& InNamespace, const FManifestContext& InContext, FString* OutDependencyFilePath) const
{
	for (int32 DepIndex = 0; DepIndex < Dependencies.Num(); ++DepIndex)
	{
		TSharedPtr<FInternationalizationManifest> DepManifest = Dependencies[DepIndex];

		const TSharedPtr<FManifestEntry> DepEntry = DepManifest->FindEntryByContext(InNamespace, InContext);
		if (DepEntry.IsValid())
		{
			if (OutDependencyFilePath)
			{
				*OutDependencyFilePath = DependencyPaths[DepIndex];
			}
			return DepEntry;
		}
	}

	return nullptr;
}

bool FLocTextHelper::AddSourceText(const FString& InNamespace, const FLocItem& InSource, const FManifestContext& InContext, const FString* InDescription)
{
	checkf(Manifest.IsValid(), TEXT("Attempted to add source text, but no manifest has been loaded!"));
	
	bool bAddSuccessful = false;

	// Check if the entry already exists in the manifest or one of the manifest dependencies
	FString ExistingEntryFileName;
	TSharedPtr<FManifestEntry> ExistingEntry = Manifest->FindEntryByContext(InNamespace, InContext);
	if (!ExistingEntry.IsValid())
	{
		ExistingEntry = FindDependencyEntry(InNamespace, InContext, &ExistingEntryFileName);
	}

	if (ExistingEntry.IsValid())
	{
		if (InSource.IsExactMatch(ExistingEntry->Source))
		{
			bAddSuccessful = true;
		}
		else
		{
			// Grab the source location of the conflicting context
			const FManifestContext* ConflictingContext = ExistingEntry->FindContext(InContext.Key, InContext.KeyMetadataObj);
			const FString ExistingEntrySourceLocation = (!ExistingEntryFileName.IsEmpty()) ? ExistingEntryFileName : ConflictingContext->SourceLocation;

			FString Message = SanitizeLogOutput(
				FString::Printf(TEXT("Found previously entered localized string: %s [%s] %s %s=\"%s\" %s. It was previously \"%s\" %s in %s."),
					(InDescription ? **InDescription : *FString()),
					*InNamespace,
					*InContext.Key,
					*FJsonInternationalizationMetaDataSerializer::MetadataToString(InContext.KeyMetadataObj),
					*InSource.Text,
					*FJsonInternationalizationMetaDataSerializer::MetadataToString(InSource.MetadataObj),
					*ExistingEntry->Source.Text,
					*FJsonInternationalizationMetaDataSerializer::MetadataToString(ExistingEntry->Source.MetadataObj),
					*ExistingEntrySourceLocation
					)
				);
			UE_LOG(LogLocTextHelper, Warning, TEXT("%s"), *Message);

			ConflictTracker.AddConflict(InNamespace, InContext.Key, InContext.KeyMetadataObj, InSource, InContext.SourceLocation);
			ConflictTracker.AddConflict(InNamespace, InContext.Key, InContext.KeyMetadataObj, ExistingEntry->Source, ExistingEntrySourceLocation);
		}
	}
	else
	{
		bAddSuccessful = Manifest->AddSource(InNamespace, InSource, InContext);
		if (!bAddSuccessful)
		{
			UE_LOG(LogLocTextHelper, Error, TEXT("Could not process localized string: %s [%s] %s=\"%s\" %s."),
				(InDescription ? **InDescription : *FString()),
				*InNamespace,
				*InContext.Key,
				*InSource.Text,
				*FJsonInternationalizationMetaDataSerializer::MetadataToString(InSource.MetadataObj)
				);
		}
	}

	return bAddSuccessful;
}

void FLocTextHelper::UpdateSourceText(const TSharedRef<FManifestEntry>& InOldEntry, TSharedRef<FManifestEntry>& InNewEntry)
{
	checkf(Manifest.IsValid(), TEXT("Attempted to update source text, but no manifest has been loaded!"));
	Manifest->UpdateEntry(InOldEntry, InNewEntry);
}

TSharedPtr<FManifestEntry> FLocTextHelper::FindSourceText(const FString& InNamespace, const FString& InKey, const FString* InSourceText) const
{
	checkf(Manifest.IsValid(), TEXT("Attempted to find source text, but no manifest has been loaded!"));
	return Manifest->FindEntryByKey(InNamespace, InKey, InSourceText);
}

TSharedPtr<FManifestEntry> FLocTextHelper::FindSourceText(const FString& InNamespace, const FManifestContext& InContext) const
{
	checkf(Manifest.IsValid(), TEXT("Attempted to find source text, but no manifest has been loaded!"));
	return Manifest->FindEntryByContext(InNamespace, InContext);
}

void FLocTextHelper::EnumerateSourceTexts(const FEnumerateSourceTextsFuncPtr& InCallback, const bool InCheckDependencies) const
{
	checkf(Manifest.IsValid(), TEXT("Attempted to enumerate source texts, but no manifest has been loaded!"));

	for (FManifestEntryByStringContainer::TConstIterator It(Manifest->GetEntriesByKeyIterator()); It; ++It)
	{
		const TSharedRef<FManifestEntry> ManifestEntry = It.Value();

		bool bShouldEnumerate = true;
		if (InCheckDependencies)
		{
			for (const TSharedPtr<FInternationalizationManifest>& DepManifest : Dependencies)
			{
				const TSharedPtr<FManifestEntry> DepEntry = DepManifest->FindEntryBySource(ManifestEntry->Namespace, ManifestEntry->Source);
				if (DepEntry.IsValid())
				{
					bShouldEnumerate = false;
					break;
				}
			}
		}

		if (bShouldEnumerate && !InCallback(ManifestEntry))
		{
			break;
		}
	}
}

bool FLocTextHelper::AddTranslation(const FString& InCulture, const FString& InNamespace, const FString& InKey, const TSharedPtr<FLocMetadataObject>& InKeyMetadataObj, const FLocItem& InSource, const FLocItem& InTranslation, const bool InOptional)
{
	TSharedPtr<FInternationalizationArchive> Archive = Archives.FindRef(InCulture);
	checkf(Archive.IsValid(), TEXT("Attempted to add a translation, but no valid archive could be found for '%s'!"), *InCulture);
	return Archive->AddEntry(InNamespace, InKey, InSource, InTranslation, InKeyMetadataObj, InOptional);
}

bool FLocTextHelper::AddTranslation(const FString& InCulture, const TSharedRef<FArchiveEntry>& InEntry)
{
	TSharedPtr<FInternationalizationArchive> Archive = Archives.FindRef(InCulture);
	checkf(Archive.IsValid(), TEXT("Attempted to add a translation, but no valid archive could be found for '%s'!"), *InCulture);
	return Archive->AddEntry(InEntry);
}

bool FLocTextHelper::UpdateTranslation(const FString& InCulture, const FString& InNamespace, const FString& InKey, const TSharedPtr<FLocMetadataObject>& InKeyMetadataObj, const FLocItem& InSource, const FLocItem& InTranslation)
{
	TSharedPtr<FInternationalizationArchive> Archive = Archives.FindRef(InCulture);
	checkf(Archive.IsValid(), TEXT("Attempted to update a translation, but no valid archive could be found for '%s'!"), *InCulture);
	return Archive->SetTranslation(InNamespace, InKey, InSource, InTranslation, InKeyMetadataObj);
}

void FLocTextHelper::UpdateTranslation(const FString& InCulture, const TSharedRef<FArchiveEntry>& InOldEntry, const TSharedRef<FArchiveEntry>& InNewEntry)
{
	TSharedPtr<FInternationalizationArchive> Archive = Archives.FindRef(InCulture);
	checkf(Archive.IsValid(), TEXT("Attempted to update a translation, but no valid archive could be found for '%s'!"), *InCulture);
	Archive->UpdateEntry(InOldEntry, InNewEntry);
}

bool FLocTextHelper::ImportTranslation(const FString& InCulture, const FString& InNamespace, const FString& InKey, const TSharedPtr<FLocMetadataObject> InKeyMetadataObj, const FLocItem& InSource, const FLocItem& InTranslation, const bool InOptional)
{
	TSharedPtr<FInternationalizationArchive> Archive = Archives.FindRef(InCulture);
	checkf(Archive.IsValid(), TEXT("Attempted to update a translation, but no valid archive could be found for '%s'!"), *InCulture);

	// First try and update an existing entry...
	if (Archive->SetTranslation(InNamespace, InKey, InSource, InTranslation, InKeyMetadataObj))
	{
		return true;
	}

	// ... failing that, try to add a new entry
	return Archive->AddEntry(InNamespace, InKey, InSource, InTranslation, InKeyMetadataObj, InOptional);
}

TSharedPtr<FArchiveEntry> FLocTextHelper::FindTranslation(const FString& InCulture, const FString& InNamespace, const FString& InKey, const TSharedPtr<FLocMetadataObject> InKeyMetadataObj) const
{
	return FindTranslationImpl(InCulture, InNamespace, InKey, InKeyMetadataObj);
}

void FLocTextHelper::EnumerateTranslations(const FString& InCulture, const FEnumerateTranslationsFuncPtr& InCallback, const bool InCheckDependencies) const
{
	TSharedPtr<FInternationalizationArchive> Archive = Archives.FindRef(InCulture);
	checkf(Archive.IsValid(), TEXT("Attempted to enumerate translations, but no valid archive could be found for '%s'!"), *InCulture);

	EnumerateSourceTexts([&](TSharedRef<FManifestEntry> InManifestEntry) -> bool
	{
		bool bContinue = true;
		
		for (const FManifestContext& ManifestContext : InManifestEntry->Contexts)
		{
			TSharedPtr<FArchiveEntry> ArchiveEntry = FindTranslation(InCulture, InManifestEntry->Namespace, ManifestContext.Key, ManifestContext.KeyMetadataObj);
			if (ArchiveEntry.IsValid() && !InCallback(ArchiveEntry.ToSharedRef()))
			{
				bContinue = false;
				break;
			}
		}

		return bContinue;
	}, InCheckDependencies);
}

void FLocTextHelper::GetExportText(const FString& InCulture, const FString& InNamespace, const FString& InKey, const TSharedPtr<FLocMetadataObject> InKeyMetadataObj, const ELocTextExportSourceMethod InSourceMethod, const FLocItem& InSource, FLocItem& OutSource, FLocItem& OutTranslation) const
{
	// Default to the raw source text for the case where we're not using native translations as source
	OutSource = InSource;
	OutTranslation = FLocItem();

	if (InSourceMethod == ELocTextExportSourceMethod::NativeText && !NativeCulture.IsEmpty() && InCulture != NativeCulture)
	{
		TSharedPtr<FArchiveEntry> NativeArchiveEntry = FindTranslationImpl(NativeCulture, InNamespace, InKey, InKeyMetadataObj);
		if (NativeArchiveEntry.IsValid() && !NativeArchiveEntry->Source.IsExactMatch(NativeArchiveEntry->Translation))
		{
			// Use the native translation as the source
			OutSource = NativeArchiveEntry->Translation;
		}
	}

	TSharedPtr<FArchiveEntry> ArchiveEntry = FindTranslationImpl(InCulture, InNamespace, InKey, InKeyMetadataObj);
	if (ArchiveEntry.IsValid())
	{
		// Set the export text to use the current translation if the entry source matches the export source
		if (ArchiveEntry->Source.IsExactMatch(OutSource))
		{
			OutTranslation = ArchiveEntry->Translation;
		}
	}
	
	// We use the source text as the default translation for the native culture
	if (OutTranslation.Text.IsEmpty() && !NativeCulture.IsEmpty() && InCulture == NativeCulture)
	{
		OutTranslation = OutSource;
	}
}

void FLocTextHelper::GetRuntimeText(const FString& InCulture, const FString& InNamespace, const FString& InKey, const TSharedPtr<FLocMetadataObject> InKeyMetadataObj, const ELocTextExportSourceMethod InSourceMethod, const FLocItem& InSource, FLocItem& OutTranslation, const bool bSkipSourceCheck) const
{
	OutTranslation = InSource;

	TSharedPtr<FArchiveEntry> ArchiveEntry = FindTranslationImpl(InCulture, InNamespace, InKey, InKeyMetadataObj);
	if (ArchiveEntry.IsValid())
	{
		if (bSkipSourceCheck)
		{
			// Set the export text to use the current translation
			OutTranslation = ArchiveEntry->Translation;
		}
		else
		{
			FLocItem ExpectedSource = InSource;

			if (InSourceMethod == ELocTextExportSourceMethod::NativeText && !NativeCulture.IsEmpty() && InCulture != NativeCulture)
			{
				TSharedPtr<FArchiveEntry> NativeArchiveEntry = FindTranslationImpl(NativeCulture, InNamespace, InKey, InKeyMetadataObj);
				if (NativeArchiveEntry.IsValid() && !NativeArchiveEntry->Source.IsExactMatch(NativeArchiveEntry->Translation))
				{
					// Use the native translation as the source
					ExpectedSource = NativeArchiveEntry->Translation;
				}
			}

			if (ArchiveEntry->Source.IsExactMatch(ExpectedSource))
			{
				// Set the export text to use the current translation
				OutTranslation = ArchiveEntry->Translation;
			}
		}
	}
}

FString FLocTextHelper::GetConflictReport() const
{
	return ConflictTracker.GetConflictReport();
}

FString FLocTextHelper::SanitizeLogOutput(const FString& InString)
{
	if (InString.IsEmpty())
	{
		return InString;
	}

	FString ResultStr = InString.ReplaceCharWithEscapedChar();
	if (!GIsBuildMachine)
	{
		return ResultStr;
	}

	static TArray<FString> ErrorStrs;
	if (ErrorStrs.Num() == 0)
	{
		ErrorStrs.Add(FString(TEXT("Error")));
		ErrorStrs.Add(FString(TEXT("Failed")));
		ErrorStrs.Add(FString(TEXT("[BEROR]")));
		ErrorStrs.Add(FString(TEXT("Utility finished with exit code: -1")));
		ErrorStrs.Add(FString(TEXT("is not recognized as an internal or external command")));
		ErrorStrs.Add(FString(TEXT("Could not open solution: ")));
		ErrorStrs.Add(FString(TEXT("Parameter format not correct")));
		ErrorStrs.Add(FString(TEXT("Another build is already started on this computer.")));
		ErrorStrs.Add(FString(TEXT("Sorry but the link was not completed because memory was exhausted.")));
		ErrorStrs.Add(FString(TEXT("simply rerunning the compiler might fix this problem")));
		ErrorStrs.Add(FString(TEXT("No connection could be made because the target machine actively refused")));
		ErrorStrs.Add(FString(TEXT("Internal Linker Exception:")));
		ErrorStrs.Add(FString(TEXT(": warning LNK4019: corrupt string table")));
		ErrorStrs.Add(FString(TEXT("Proxy could not update its cache")));
		ErrorStrs.Add(FString(TEXT("You have not agreed to the Xcode license agreements")));
		ErrorStrs.Add(FString(TEXT("Connection to build service terminated")));
		ErrorStrs.Add(FString(TEXT("cannot execute binary file")));
		ErrorStrs.Add(FString(TEXT("Invalid solution configuration")));
		ErrorStrs.Add(FString(TEXT("is from a previous version of this application and must be converted in order to build")));
		ErrorStrs.Add(FString(TEXT("This computer has not been authenticated for your account using Steam Guard")));
		ErrorStrs.Add(FString(TEXT("invalid name for SPA section")));
		ErrorStrs.Add(FString(TEXT(": Invalid file name, ")));
		ErrorStrs.Add(FString(TEXT("The specified PFX file do not exist. Aborting")));
		ErrorStrs.Add(FString(TEXT("binary is not found. Aborting")));
		ErrorStrs.Add(FString(TEXT("Input file not found: ")));
		ErrorStrs.Add(FString(TEXT("An exception occurred during merging:")));
		ErrorStrs.Add(FString(TEXT("Install the 'Microsoft Windows SDK for Windows 7 and .NET Framework 3.5 SP1'")));
		ErrorStrs.Add(FString(TEXT("is less than package's new version 0x")));
		ErrorStrs.Add(FString(TEXT("current engine version is older than version the package was originally saved with")));
		ErrorStrs.Add(FString(TEXT("exceeds maximum length")));
		ErrorStrs.Add(FString(TEXT("can't edit exclusive file already opened")));
	}

	for (int32 ErrStrIdx = 0; ErrStrIdx < ErrorStrs.Num(); ErrStrIdx++)
	{
		FString& FindStr = ErrorStrs[ErrStrIdx];
		FString ReplaceStr = FString::Printf(TEXT("%s %s"), *FindStr.Left(1), *FindStr.RightChop(1));

		ResultStr.ReplaceInline(*FindStr, *ReplaceStr);
	}

	return ResultStr;
}

bool FLocTextHelper::FindKeysForLegacyTranslation(const FString& InCulture, const FString& InNamespace, const FString& InSource, const TSharedPtr<FLocMetadataObject> InKeyMetadataObj, TArray<FString>& OutKeys) const
{
	checkf(Manifest.IsValid(), TEXT("Attempted to find a key for a legacy translation, but no manifest has been loaded!"));

	TSharedPtr<FInternationalizationArchive> NativeArchive;
	if (!NativeCulture.IsEmpty() && InCulture != NativeCulture)
	{
		NativeArchive = Archives.FindRef(NativeCulture);
		checkf(NativeArchive.IsValid(), TEXT("Attempted to find a key for a legacy translation, but no valid archive could be found for '%s'!"), *NativeCulture);
	}

	return FindKeysForLegacyTranslation(Manifest.ToSharedRef(), NativeArchive, InNamespace, InSource, InKeyMetadataObj, OutKeys);
}

bool FLocTextHelper::FindKeysForLegacyTranslation(const TSharedRef<const FInternationalizationManifest>& InManifest, const TSharedPtr<const FInternationalizationArchive>& InNativeArchive, const FString& InNamespace, const FString& InSource, const TSharedPtr<FLocMetadataObject> InKeyMetadataObj, TArray<FString>& OutKeys)
{
	FString RealSourceText = InSource;

	// The source text may be a native translation, so we first need to check the native archive to find the real source text that will exist in the manifest
	if (InNativeArchive.IsValid())
	{
		// We don't maintain a translation -> source mapping, so we just have to brute force it
		for (FArchiveEntryByStringContainer::TConstIterator It = InNativeArchive->GetEntriesBySourceTextIterator(); It; ++It)
		{
			const TSharedRef<FArchiveEntry>& ArchiveEntry = It.Value();
			if (ArchiveEntry->Namespace.Equals(InNamespace, ESearchCase::CaseSensitive) && ArchiveEntry->Translation.Text.Equals(InSource, ESearchCase::CaseSensitive))
			{
				if (!ArchiveEntry->KeyMetadataObj.IsValid() && !InKeyMetadataObj.IsValid())
				{
					RealSourceText = ArchiveEntry->Source.Text;
					break;
				}
				else if ((InKeyMetadataObj.IsValid() != ArchiveEntry->KeyMetadataObj.IsValid()))
				{
					// If we are in here, we know that one of the metadata entries is null, if the other contains zero entries we will still consider them equivalent.
					if ((InKeyMetadataObj.IsValid() && InKeyMetadataObj->Values.Num() == 0) || (ArchiveEntry->KeyMetadataObj.IsValid() && ArchiveEntry->KeyMetadataObj->Values.Num() == 0))
					{
						RealSourceText = ArchiveEntry->Source.Text;
						break;
					}
				}
				else if (*ArchiveEntry->KeyMetadataObj == *InKeyMetadataObj)
				{
					RealSourceText = ArchiveEntry->Source.Text;
					break;
				}
			}
		}
	}

	bool bFoundKeys = false;

	TSharedPtr<FManifestEntry> ManifestEntry = InManifest->FindEntryBySource(InNamespace, FLocItem(RealSourceText));
	if (ManifestEntry.IsValid())
	{
		for (const FManifestContext& Context : ManifestEntry->Contexts)
		{
			if (Context.KeyMetadataObj.IsValid() != InKeyMetadataObj.IsValid())
			{
				continue;
			}
			else if ((!Context.KeyMetadataObj.IsValid() && !InKeyMetadataObj.IsValid()) || (*Context.KeyMetadataObj == *InKeyMetadataObj))
			{
				OutKeys.Add(Context.Key);
				bFoundKeys = true;
			}
		}
	}

	return bFoundKeys;
}

TSharedPtr<FInternationalizationManifest> FLocTextHelper::LoadManifestImpl(const FString& InManifestFilePath, const ELocTextHelperLoadFlags InLoadFlags, FText* OutError)
{
	TSharedPtr<FInternationalizationManifest> LocalManifest = MakeShareable(new FInternationalizationManifest());

	// Attempt to load an existing manifest first
	if (!!(InLoadFlags & ELocTextHelperLoadFlags::Load))
	{
		bool bLoaded = false;
		const bool bExisted = FPaths::FileExists(InManifestFilePath);

		if (bExisted)
		{
			if (LocFileNotifies.IsValid())
			{
				LocFileNotifies->PreFileRead(InManifestFilePath);
			}

			if (FJsonInternationalizationManifestSerializer::DeserializeManifestFromFile(InManifestFilePath, LocalManifest.ToSharedRef()))
			{
				bLoaded = true;
			}
			else
			{
				if (OutError)
				{
					*OutError = FText::Format(LOCTEXT("Error_LoadManifest_DeserializeFile", "Failed to deserialize manifest '{0}'."), FText::FromString(InManifestFilePath));
				}
			}

			if (LocFileNotifies.IsValid())
			{
				LocFileNotifies->PostFileRead(InManifestFilePath);
			}
		}

		if (bLoaded)
		{
			return LocalManifest;
		}
		else if (bExisted)
		{
			// Don't allow fallback to Create if the file exists but could not be loaded
			return nullptr;
		}
	}

	// If we're allowed to create a manifest than we can never fail
	if (!!(InLoadFlags & ELocTextHelperLoadFlags::Create))
	{
		return LocalManifest;
	}

	return nullptr;
}

bool FLocTextHelper::SaveManifestImpl(const TSharedRef<const FInternationalizationManifest>& InManifest, const FString& InManifestFilePath, FText* OutError) const
{
	bool bSaved = false;

	if (LocFileNotifies.IsValid())
	{
		LocFileNotifies->PreFileWrite(InManifestFilePath);
	}

	TSharedRef<FJsonObject> ManifestJsonObject = MakeShareable(new FJsonObject());
	if (FJsonInternationalizationManifestSerializer::SerializeManifestToFile(InManifest, InManifestFilePath))
	{
		bSaved = true;
	}
	else
	{
		if (OutError)
		{
			*OutError = FText::Format(LOCTEXT("Error_SaveManifest_SerializeFile", "Failed to serialize manifest '{0}'."), FText::FromString(InManifestFilePath));
		}
	}

	if (LocFileNotifies.IsValid())
	{
		LocFileNotifies->PostFileWrite(InManifestFilePath);
	}

	return bSaved;
}

TSharedPtr<FInternationalizationArchive> FLocTextHelper::LoadArchiveImpl(const FString& InArchiveFilePath, const ELocTextHelperLoadFlags InLoadFlags, FText* OutError)
{
	TSharedPtr<FInternationalizationArchive> LocalArchive = MakeShareable(new FInternationalizationArchive());

	// Attempt to load an existing archive first
	if (!!(InLoadFlags & ELocTextHelperLoadFlags::Load))
	{
		bool bLoaded = false;
		const bool bExisted = FPaths::FileExists(InArchiveFilePath);

		if (bExisted)
		{
			if (LocFileNotifies.IsValid())
			{
				LocFileNotifies->PreFileRead(InArchiveFilePath);
			}

			TSharedPtr<FInternationalizationArchive> NativeArchive;
			if (!NativeCulture.IsEmpty())
			{
				NativeArchive = Archives.FindRef(NativeCulture);
			}
			
			if (FJsonInternationalizationArchiveSerializer::DeserializeArchiveFromFile(InArchiveFilePath, LocalArchive.ToSharedRef(), Manifest, NativeArchive))
			{
				bLoaded = true;
			}
			else
			{
				if (OutError)
				{
					*OutError = FText::Format(LOCTEXT("Error_LoadArchive_DeserializeFile", "Failed to deserialize archive '{0}'."), FText::FromString(InArchiveFilePath));
				}
			}

			if (LocFileNotifies.IsValid())
			{
				LocFileNotifies->PostFileRead(InArchiveFilePath);
			}
		}

		if (bLoaded)
		{
			return LocalArchive;
		}
		else if (bExisted)
		{
			// Don't allow fallback to Create if the file exists but could not be loaded
			return nullptr;
		}
	}

	// If we're allowed to create a manifest than we can never fail
	if (!!(InLoadFlags & ELocTextHelperLoadFlags::Create))
	{
		return LocalArchive;
	}

	return nullptr;
}

bool FLocTextHelper::SaveArchiveImpl(const TSharedRef<const FInternationalizationArchive>& InArchive, const FString& InArchiveFilePath, FText* OutError) const
{
	bool bSaved = false;

	if (LocFileNotifies.IsValid())
	{
		LocFileNotifies->PreFileWrite(InArchiveFilePath);
	}

	if (FJsonInternationalizationArchiveSerializer::SerializeArchiveToFile(InArchive, InArchiveFilePath))
	{
		bSaved = true;
	}
	else
	{
		if (OutError)
		{
			*OutError = FText::Format(LOCTEXT("Error_SaveArchive_SerializeFile", "Failed to serialize archive '{0}'."), FText::FromString(InArchiveFilePath));
		}
	}

	if (LocFileNotifies.IsValid())
	{
		LocFileNotifies->PostFileWrite(InArchiveFilePath);
	}

	return bSaved;
}

TSharedPtr<FArchiveEntry> FLocTextHelper::FindTranslationImpl(const FString& InCulture, const FString& InNamespace, const FString& InKey, const TSharedPtr<FLocMetadataObject> InKeyMetadataObj) const
{
	TSharedPtr<FInternationalizationArchive> Archive = Archives.FindRef(InCulture);
	checkf(Archive.IsValid(), TEXT("Attempted to find a translation, but no valid archive could be found for '%s'!"), *InCulture);
	return Archive->FindEntryByKey(InNamespace, InKey, InKeyMetadataObj);
}

#undef LOCTEXT_NAMESPACE
