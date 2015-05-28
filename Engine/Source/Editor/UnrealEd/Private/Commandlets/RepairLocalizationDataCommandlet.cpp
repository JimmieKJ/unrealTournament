// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Commandlets/RepairLocalizationDataCommandlet.h"
#include "InternationalizationManifest.h"
#include "InternationalizationArchive.h"
#include "JsonInternationalizationManifestSerializer.h"
#include "JsonInternationalizationArchiveSerializer.h"
#include "Regex.h"

DEFINE_LOG_CATEGORY_STATIC(LogRepairLocalizationDataCommandlet, Log, All);

void RepairManifestAndArchives(TSharedRef<FInternationalizationManifest> Manifest, TArray< TSharedRef<FInternationalizationArchive> > Archives)
{
	// Update source text if this manifest was saved before the escape sequence fixes.
	if (Manifest->GetFormatVersion() < FInternationalizationManifest::EFormatVersion::EscapeFixes)
	{
		TManifestEntryBySourceTextContainer::TConstIterator Iterator = Manifest->GetEntriesBySourceTextIterator();
		for(; Iterator; ++Iterator)
		{
			const FString& Source = Iterator.Key();
			const TSharedRef<FManifestEntry>& ManifestEntry = Iterator.Value();

			// Find double quotes and unescape them once.
			FLocItem UpdatedSource(ManifestEntry->Source);
			UpdatedSource.Text.ReplaceInline(TEXT("\\\""), TEXT("\""));

			TSharedRef<FManifestEntry> UpdatedManifestEntry = MakeShareable(new FManifestEntry(ManifestEntry->Namespace, UpdatedSource));
			UpdatedManifestEntry->Contexts = ManifestEntry->Contexts;

			Manifest->UpdateEntry(ManifestEntry, UpdatedManifestEntry);
		}
	}

	for (const TSharedRef<FInternationalizationArchive>& Archive : Archives)
	{
		// Update source text if this archive was saved before the escape sequence fixes.
		if (Archive->GetFormatVersion() < FInternationalizationArchive::EFormatVersion::EscapeFixes)
		{
			TArchiveEntryContainer::TConstIterator Iterator = Archive->GetEntryIterator();

			for (; Iterator; ++Iterator)
			{
				const FString& Source = Iterator.Key();
				const TSharedRef<FArchiveEntry>& ArchiveEntry = Iterator.Value();

				// Find double quotes and unescape them once.
				FLocItem UpdatedSource(ArchiveEntry->Source);
				UpdatedSource.Text.ReplaceInline(TEXT("\\\""), TEXT("\""));

				// Find double quotes and unescape them once.
				FLocItem UpdatedTranslation(ArchiveEntry->Translation);
				UpdatedTranslation.Text.ReplaceInline(TEXT("\\\""), TEXT("\""));

				TSharedRef<FArchiveEntry> UpdatedArchiveEntry = MakeShareable(new FArchiveEntry(ArchiveEntry->Namespace, UpdatedSource, UpdatedTranslation, ArchiveEntry->KeyMetadataObj, ArchiveEntry->bIsOptional));

				Archive->UpdateEntry(ArchiveEntry, UpdatedArchiveEntry);
			}
		}
	}

	const FRegexPattern Pattern(TEXT(".* - line \\d+"));

	TManifestEntryBySourceTextContainer::TConstIterator Iterator = Manifest->GetEntriesBySourceTextIterator();
	for(; Iterator; ++Iterator)
	{
		const FString& Source = Iterator.Key();
		const TSharedRef<FManifestEntry>& ManifestEntry = Iterator.Value();

		// Identify if this entry comes from source text only.
		bool AreAllContextsFromSource = ManifestEntry->Contexts.Num() > 0;
		for (const FContext& Context : ManifestEntry->Contexts)
		{
			FRegexMatcher Matcher(Pattern, Context.SourceLocation);
			AreAllContextsFromSource = Matcher.FindNext();

			if(!AreAllContextsFromSource)
			{
				break;
			}
		}

		// No updates needed for this entry if it isn't all from source.
		if(!AreAllContextsFromSource)
		{
			continue;
		}

		for (const TSharedRef<FInternationalizationArchive>& Archive : Archives)
		{
			// Update source text if this manifest was saved before the escape sequence fixes.
			if (Archive->GetFormatVersion() < FInternationalizationArchive::EFormatVersion::EscapeFixes)
			{
				const TSharedPtr<FArchiveEntry> ArchiveEntry = Archive->FindEntryBySource(ManifestEntry->Namespace, ManifestEntry->Source, /*KeyMetadataObj*/ nullptr);
				if (!ArchiveEntry.IsValid())
				{
					continue;
				}

				// Replace escape sequences with their associated character, once.
				FLocItem UpdatedSource(ArchiveEntry->Source);
				UpdatedSource.Text = UpdatedSource.Text.ReplaceEscapedCharWithChar();

				// Replace escape sequences with their associated character, once.
				FLocItem UpdatedTranslation(ArchiveEntry->Translation);
				UpdatedTranslation.Text = UpdatedTranslation.Text.ReplaceEscapedCharWithChar();

				TSharedRef<FArchiveEntry> UpdatedArchiveEntry = MakeShareable(new FArchiveEntry(ArchiveEntry->Namespace, UpdatedSource, UpdatedTranslation, ArchiveEntry->KeyMetadataObj, ArchiveEntry->bIsOptional));

				Archive->UpdateEntry(ArchiveEntry.ToSharedRef(), UpdatedArchiveEntry);
			}
		}

		// Update source text if this manifest was saved before the escape sequence fixes.
		if (Manifest->GetFormatVersion() < FInternationalizationManifest::EFormatVersion::EscapeFixes)
		{
			// Replace escape sequences with their associated character, once.
			FLocItem UpdatedSource(ManifestEntry->Source);
			UpdatedSource.Text = UpdatedSource.Text.ReplaceEscapedCharWithChar();

			TSharedRef<FManifestEntry> UpdatedManifestEntry = MakeShareable(new FManifestEntry(ManifestEntry->Namespace, UpdatedSource));
			UpdatedManifestEntry->Contexts = ManifestEntry->Contexts;

			Manifest->UpdateEntry(ManifestEntry, UpdatedManifestEntry);
		}
	}

	Manifest->SetFormatVersion(FInternationalizationManifest::EFormatVersion::Latest);
	for (const TSharedRef<FInternationalizationArchive>& Archive : Archives)
	{
		Archive->SetFormatVersion(FInternationalizationArchive::EFormatVersion::Latest);
	}
}

URepairLocalizationDataCommandlet::URepairLocalizationDataCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 URepairLocalizationDataCommandlet::Main(const FString& Params)
{
	FInternationalization& I18N = FInternationalization::Get();

	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	//Set config file
	const FString* ParamVal = ParamVals.Find(FString(TEXT("Config")));
	FString GatherTextConfigPath;

	if ( ParamVal )
	{
		GatherTextConfigPath = *ParamVal;
	}
	else
	{
		UE_LOG(LogRepairLocalizationDataCommandlet, Error, TEXT("No config specified."));
		return -1;
	}

	//Set config section
	ParamVal = ParamVals.Find(FString(TEXT("Section")));
	FString SectionName;

	if ( ParamVal )
	{
		SectionName = *ParamVal;
	}
	else
	{
		UE_LOG(LogRepairLocalizationDataCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	// Get destination path.
	FString DestinationPath;
	if( !GetPathFromConfig( *SectionName, TEXT("DestinationPath"), DestinationPath, GatherTextConfigPath ) )
	{
		UE_LOG( LogRepairLocalizationDataCommandlet, Error, TEXT("No destination path specified.") );
		return -1;
	}

	// Get manifest name.
	FString ManifestName;
	if( !GetStringFromConfig( *SectionName, TEXT("ManifestName"), ManifestName, GatherTextConfigPath ) )
	{
		UE_LOG( LogRepairLocalizationDataCommandlet, Error, TEXT("No manifest name specified.") );
		return -1;
	}

	// Get archive name.
	FString ArchiveName;
	if( !( GetStringFromConfig(* SectionName, TEXT("ArchiveName"), ArchiveName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogRepairLocalizationDataCommandlet, Error, TEXT("No archive name specified."));
		return -1;
	}

	// Get cultures to generate.
	TArray<FString> CulturesToGenerate;
	GetStringArrayFromConfig(*SectionName, TEXT("CulturesToGenerate"), CulturesToGenerate, GatherTextConfigPath);

	if( CulturesToGenerate.Num() == 0 )
	{
		UE_LOG(LogRepairLocalizationDataCommandlet, Error, TEXT("No cultures specified for generation."));
		return -1;
	}

	for(int32 i = 0; i < CulturesToGenerate.Num(); ++i)
	{
		if( I18N.GetCulture( CulturesToGenerate[i] ).IsValid() )
		{
			UE_LOG(LogRepairLocalizationDataCommandlet, Verbose, TEXT("Specified culture is not a valid runtime culture, but may be a valid base language: %s"), *(CulturesToGenerate[i]) );
		}
	}


	//////////////////////////////////////////////////////////////////////////


	// Read the damaged manifest.
	const FString ManifestFilePath = DestinationPath / ManifestName;
	const TSharedPtr<FJsonObject> ManifestJsonObject = ReadJSONTextFile( ManifestFilePath );
	if( !ManifestJsonObject.IsValid() )
	{
		UE_LOG(LogRepairLocalizationDataCommandlet, Error, TEXT("Could not read manifest file %s."), *ManifestFilePath);
		return -1;
	}
	FJsonInternationalizationManifestSerializer ManifestSerializer;
	const TSharedRef< FInternationalizationManifest > InternationalizationManifest = MakeShareable( new FInternationalizationManifest );
	ManifestSerializer.DeserializeManifest( ManifestJsonObject.ToSharedRef(), InternationalizationManifest );

	// Read the damaged archives.
	TArray< TSharedRef<FInternationalizationArchive> > InternationalizationArchives;
	TArray<FString> ArchiveCultures;
	for(int32 Culture = 0; Culture < CulturesToGenerate.Num(); Culture++)
	{
		// Read in any existing archive for this culture.
		const FString CulturePath = DestinationPath / CulturesToGenerate[Culture];
		const FString ArchiveFilePath = CulturePath / ArchiveName;
		if( FPaths::FileExists(ArchiveFilePath) )
		{
			TSharedPtr< FJsonObject > ArchiveJsonObject = ReadJSONTextFile( ArchiveFilePath );
			if( !ArchiveJsonObject.IsValid() )
			{
				UE_LOG(LogRepairLocalizationDataCommandlet, Error, TEXT("Could not read archive file %s."), *ArchiveFilePath);
				return -1;
			}
			FJsonInternationalizationArchiveSerializer ArchiveSerializer;
			const TSharedRef< FInternationalizationArchive > InternationalizationArchive = MakeShareable( new FInternationalizationArchive );
			ArchiveSerializer.DeserializeArchive( ArchiveJsonObject.ToSharedRef(), InternationalizationArchive );

			InternationalizationArchives.Add(InternationalizationArchive);
			ArchiveCultures.Add(CulturesToGenerate[Culture]);
		}
	}

	// Repair.
	RepairManifestAndArchives(InternationalizationManifest, InternationalizationArchives);

	// Write the repaired manifests.
	TSharedRef< FJsonObject > OutputManifestJsonObj = MakeShareable( new FJsonObject );
	ManifestSerializer.SerializeManifest( InternationalizationManifest, OutputManifestJsonObj );
	if (!WriteJSONToTextFile( OutputManifestJsonObj, DestinationPath / ManifestName, SourceControlInfo ))
	{
		UE_LOG( LogRepairLocalizationDataCommandlet, Error,TEXT("Failed to write manifest to %s."), *DestinationPath );				
		return -1;
	}

	// Write the repaired archives.
	{
		int Index = 0;
		for (const TSharedRef<FInternationalizationArchive>& InternationalizationArchive : InternationalizationArchives)
		{
			TSharedRef< FJsonObject > OutputArchiveJsonObj = MakeShareable( new FJsonObject );
			FJsonInternationalizationArchiveSerializer ArchiveSerializer;
			ArchiveSerializer.SerializeArchive( InternationalizationArchive, OutputArchiveJsonObj );

			int32 Culture = 0;
			if (!WriteJSONToTextFile( OutputArchiveJsonObj, (DestinationPath / *ArchiveCultures[Index] / ArchiveName), SourceControlInfo ))
			{
				UE_LOG( LogRepairLocalizationDataCommandlet, Error,TEXT("Failed to write archive to %s."), *DestinationPath );				
				return -1;
			}
			++Index;
		}
	}

	return 0;
}