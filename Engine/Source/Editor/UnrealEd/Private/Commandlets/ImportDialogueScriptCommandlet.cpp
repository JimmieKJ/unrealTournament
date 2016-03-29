// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Commandlets/ImportDialogueScriptCommandlet.h"
#include "CsvParser.h"
#include "Sound/DialogueWave.h"
#include "Internationalization/InternationalizationManifest.h"
#include "Internationalization/InternationalizationArchive.h"
#include "JsonInternationalizationManifestSerializer.h"
#include "JsonInternationalizationArchiveSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogImportDialogueScriptCommandlet, Log, All);

UImportDialogueScriptCommandlet::UImportDialogueScriptCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UImportDialogueScriptCommandlet::Main(const FString& Params)
{
	// Parse command line
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	// Set config path
	FString ConfigPath;
	{
		const FString* ConfigPathParamVal = ParamVals.Find(FString(TEXT("Config")));
		if (!ConfigPathParamVal)
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No config specified."));
			return -1;
		}
		ConfigPath = *ConfigPathParamVal;
	}

	// Set config section
	FString SectionName;
	{
		const FString* SectionNameParamVal = ParamVals.Find(FString(TEXT("Section")));
		if (!SectionNameParamVal)
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No config section specified."));
			return -1;
		}
		SectionName = *SectionNameParamVal;
	}

	// Source path to the root folder that dialogue script CSV files live in
	FString SourcePath;
	if (!GetPathFromConfig(*SectionName, TEXT("SourcePath"), SourcePath, ConfigPath))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No source path specified."));
		return -1;
	}

	// Destination path to the root folder that manifest/archive files live in
	FString DestinationPath;
	if (!GetPathFromConfig(*SectionName, TEXT("DestinationPath"), DestinationPath, ConfigPath))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No destination path specified."));
		return -1;
	}

	// Get culture directory setting, default to true if not specified (used to allow picking of export directory with windows file dialog from Translation Editor)
	bool bUseCultureDirectory = true;
	if (!GetBoolFromConfig(*SectionName, TEXT("bUseCultureDirectory"), bUseCultureDirectory, ConfigPath))
	{
		bUseCultureDirectory = true;
	}

	// Get the native culture
	FString NativeCulture;
	if (!GetStringFromConfig(*SectionName, TEXT("NativeCulture"), NativeCulture, ConfigPath))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No native culture specified."));
		return -1;
	}

	// Get cultures to generate
	TArray<FString> CulturesToGenerate;
	if (GetStringArrayFromConfig(*SectionName, TEXT("CulturesToGenerate"), CulturesToGenerate, ConfigPath) == 0)
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No cultures specified for import."));
		return -1;
	}

	// Get the manifest name
	FString ManifestName;
	if (!GetStringFromConfig(*SectionName, TEXT("ManifestName"), ManifestName, ConfigPath))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No manifest name specified."));
		return -1;
	}

	// Get the archive name
	FString ArchiveName;
	if (!GetStringFromConfig(*SectionName, TEXT("ArchiveName"), ArchiveName, ConfigPath))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No archive name specified."));
		return -1;
	}

	// Get the dialogue script name
	FString DialogueScriptName;
	if (!GetStringFromConfig(*SectionName, TEXT("DialogueScriptName"), DialogueScriptName, ConfigPath))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("No dialogue script name specified."));
		return -1;
	}

	// Prepare the manifest
	{
		const FString ManifestFileName = DestinationPath / ManifestName;
		if (!FPaths::FileExists(ManifestFileName))
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to find manifest '%s'."), *ManifestFileName);
			return -1;
		}

		const TSharedPtr<FJsonObject> ManifestJsonObject = ReadJSONTextFile(ManifestFileName);
		if (!ManifestJsonObject.IsValid())
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to parse manifest '%s'."), *ManifestFileName);
			return -1;
		}

		FJsonInternationalizationManifestSerializer ManifestSerializer;
		InternationalizationManifest = MakeShareable(new FInternationalizationManifest());
		if (!ManifestSerializer.DeserializeManifest(ManifestJsonObject.ToSharedRef(), InternationalizationManifest.ToSharedRef()))
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to deserialize manifest '%s'."), *ManifestFileName);
			return -1;
		}
	}

	// Prepare the native archive
	{
		const FString NativeCulturePath = DestinationPath / NativeCulture;

		const FString NativeArchiveFileName = NativeCulturePath / ArchiveName;
		if (!FPaths::FileExists(NativeArchiveFileName))
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to find archive '%s'."), *NativeArchiveFileName);
			return -1;
		}

		const TSharedPtr<FJsonObject> ArchiveJsonObject = ReadJSONTextFile(NativeArchiveFileName);
		if (!ArchiveJsonObject.IsValid())
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to parse archive '%s'."), *NativeArchiveFileName);
			return -1;
		}

		FJsonInternationalizationArchiveSerializer ArchiveSerializer;
		NativeArchive = MakeShareable(new FInternationalizationArchive());
		if (!ArchiveSerializer.DeserializeArchive(ArchiveJsonObject.ToSharedRef(), NativeArchive.ToSharedRef()))
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to deserialize archive '%s'."), *NativeArchiveFileName);
			return -1;
		}
	}

	// Import the native culture first as this may trigger additional translations in foreign archives
	{
		const FString CultureSourcePath = SourcePath / (bUseCultureDirectory ? NativeCulture : TEXT(""));
		const FString CultureDestinationPath = DestinationPath / NativeCulture;
		ImportDialogueScriptForCulture(CultureSourcePath / DialogueScriptName, CultureDestinationPath / ArchiveName, NativeCulture, true);
	}

	// Import any remaining cultures
	for (const FString& CultureName : CulturesToGenerate)
	{
		// Skip the native culture as we already processed it above
		if (CultureName == NativeCulture)
		{
			continue;
		}

		const FString CultureSourcePath = SourcePath / (bUseCultureDirectory ? CultureName : TEXT(""));
		const FString CultureDestinationPath = DestinationPath / CultureName;
		ImportDialogueScriptForCulture(CultureSourcePath / DialogueScriptName, CultureDestinationPath / ArchiveName, CultureName, false);
	}

	return 0;
}

bool UImportDialogueScriptCommandlet::ImportDialogueScriptForCulture(const FString& InDialogueScriptFileName, const FString& InCultureArchiveFileName, const FString& InCultureName, const bool bIsNativeCulture)
{
	// Load dialogue script file contents to string
	FString DialogScriptFileContents;
	if (!FFileHelper::LoadFileToString(DialogScriptFileContents, *InDialogueScriptFileName))
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to load contents of dialog script file '%s' for culture '%s'."), *InDialogueScriptFileName, *InCultureName);
		return false;
	}

	// Parse dialogue script file contents
	const FCsvParser DialogScriptFileParser(DialogScriptFileContents);
	const FCsvParser::FRows& Rows = DialogScriptFileParser.GetRows();

	// Validate dialogue script row count
	if (Rows.Num() <= 0)
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Dialogue script file has insufficient rows for culture '%s'. Expected at least 1 row, got %d."), *InCultureName, Rows.Num());
		return false;
	}

	const UProperty* SpokenDialogueProperty = FDialogueScriptEntry::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FDialogueScriptEntry, SpokenDialogue));
	const UProperty* LocalizationKeysProperty = FDialogueScriptEntry::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FDialogueScriptEntry, LocalizationKeys));

	// We need the SpokenDialogue and LocalizationKeys properties in order to perform the import, so find their respective columns in the CSV data
	int32 SpokenDialogueColumnIndex = INDEX_NONE;
	int32 LocalizationKeysColumnIndex = INDEX_NONE;
	{
		const FString SpokenDialogueColumnName = SpokenDialogueProperty->GetName();
		const FString LocalizationKeysColumnName = LocalizationKeysProperty->GetName();

		const auto& HeaderRowData = Rows[0];
		for (int32 ColumnIndex = 0; ColumnIndex < HeaderRowData.Num(); ++ColumnIndex)
		{
			const TCHAR* const CellData = HeaderRowData[ColumnIndex];
			if (FCString::Stricmp(CellData, *SpokenDialogueColumnName) == 0)
			{
				SpokenDialogueColumnIndex = ColumnIndex;
			}
			else if (FCString::Stricmp(CellData, *LocalizationKeysColumnName) == 0)
			{
				LocalizationKeysColumnIndex = ColumnIndex;
			}

			if (SpokenDialogueColumnIndex != INDEX_NONE && LocalizationKeysColumnIndex != INDEX_NONE)
			{
				break;
			}
		}
	}

	if (SpokenDialogueColumnIndex == INDEX_NONE)
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Dialogue script file is missing the required column '%s' for culture '%s'."), *SpokenDialogueProperty->GetName(), *InCultureName);
		return false;
	}

	if (LocalizationKeysColumnIndex == INDEX_NONE)
	{
		UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Dialogue script file is missing the required column '%s' for culture '%s'."), *LocalizationKeysProperty->GetName(), *InCultureName);
		return false;
	}

	// Prepare the culture archive
	TSharedPtr<FInternationalizationArchive> CultureArchive;
	if (bIsNativeCulture)
	{
		CultureArchive = NativeArchive;
	}
	else
	{
		if (!FPaths::FileExists(InCultureArchiveFileName))
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to find archive '%s'."), *InCultureArchiveFileName);
			return false;
		}

		const TSharedPtr<FJsonObject> ArchiveJsonObject = ReadJSONTextFile(InCultureArchiveFileName);
		if (!ArchiveJsonObject.IsValid())
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to parse archive '%s'."), *InCultureArchiveFileName);
			return false;
		}

		FJsonInternationalizationArchiveSerializer ArchiveSerializer;
		CultureArchive = MakeShareable(new FInternationalizationArchive());
		if (!ArchiveSerializer.DeserializeArchive(ArchiveJsonObject.ToSharedRef(), CultureArchive.ToSharedRef()))
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to deserialize archive '%s'."), *InCultureArchiveFileName);
			return false;
		}
	}

	bool bHasUpdatedArchive = false;

	// Parse each row of the CSV data
	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		const auto& RowData = Rows[RowIndex];

		FDialogueScriptEntry ParsedScriptEntry;

		// Parse the SpokenDialogue data
		{
			const TCHAR* const CellData = RowData[SpokenDialogueColumnIndex];
			if (SpokenDialogueProperty->ImportText(CellData, SpokenDialogueProperty->ContainerPtrToValuePtr<void>(&ParsedScriptEntry), PPF_None, nullptr) == nullptr)
			{
				UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to parse the required column '%s' for row '%d' for culture '%s'."), *SpokenDialogueProperty->GetName(), RowIndex, *InCultureName);
				continue;
			}
		}

		// Parse the LocalizationKeys data
		{
			const TCHAR* const CellData = RowData[LocalizationKeysColumnIndex];
			if (LocalizationKeysProperty->ImportText(CellData, LocalizationKeysProperty->ContainerPtrToValuePtr<void>(&ParsedScriptEntry), PPF_None, nullptr) == nullptr)
			{
				UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to parse the required column '%s' for row '%d' for culture '%s'."), *LocalizationKeysProperty->GetName(), RowIndex, *InCultureName);
				continue;
			}
		}

		for (const FString& ContextLocalizationKey : ParsedScriptEntry.LocalizationKeys)
		{
			// Find the manifest entry so that we can find the corresponding archive entry
			TSharedPtr<FManifestEntry> ContextManifestEntry = InternationalizationManifest->FindEntryByKey(FDialogueConstants::DialogueNamespace, ContextLocalizationKey);
			if (!ContextManifestEntry.IsValid())
			{
				UE_LOG(LogImportDialogueScriptCommandlet, Log, TEXT("No internationalization manifest entry was found for context '%s' in culture '%s'. This context will be skipped."), *ContextLocalizationKey, *InCultureName);
				continue;
			}

			// Find the correct entry for our context
			const FContext* ContextManifestEntryContext = ContextManifestEntry->FindContextByKey(ContextLocalizationKey);
			check(ContextManifestEntryContext); // This should never fail as we pass in the key to FindEntryByKey

			// Find the correct source text (we might have a native translation that we should update instead of the source)
			FString SourceText = ContextManifestEntry->Source.Text;
			if (!bIsNativeCulture)
			{
				TSharedPtr<FArchiveEntry> NativeArchiveEntry = NativeArchive->FindEntryBySource(FDialogueConstants::DialogueNamespace, SourceText, ContextManifestEntryContext->KeyMetadataObj);
				if (NativeArchiveEntry.IsValid())
				{
					SourceText = NativeArchiveEntry->Translation.Text;
				}
			}

			// Update (or add) the entry in the archive
			TSharedPtr<FArchiveEntry> ArchiveEntry = CultureArchive->FindEntryBySource(FDialogueConstants::DialogueNamespace, SourceText, ContextManifestEntryContext->KeyMetadataObj);
			if (ArchiveEntry.IsValid())
			{
				if (!ArchiveEntry->Translation.Text.Equals(ParsedScriptEntry.SpokenDialogue, ESearchCase::CaseSensitive))
				{
					bHasUpdatedArchive = true;
					ArchiveEntry->Translation.Text = ParsedScriptEntry.SpokenDialogue;
				}
			}
			else
			{
				if (CultureArchive->AddEntry(FDialogueConstants::DialogueNamespace, SourceText, ParsedScriptEntry.SpokenDialogue, ContextManifestEntryContext->KeyMetadataObj, false))
				{
					bHasUpdatedArchive = true;
				}
			}
		}
	}

	// Write out the updated archive file
	if (bHasUpdatedArchive)
	{
		TSharedRef<FJsonObject> ArchiveJsonObj = MakeShareable(new FJsonObject());
		FJsonInternationalizationArchiveSerializer ArchiveSerializer;
		ArchiveSerializer.SerializeArchive(CultureArchive.ToSharedRef(), ArchiveJsonObj);

		if (!WriteJSONToTextFile(ArchiveJsonObj, InCultureArchiveFileName, SourceControlInfo))
		{
			UE_LOG(LogImportDialogueScriptCommandlet, Error, TEXT("Failed to write archive file for culture '%s' to '%s'."), *InCultureName, *InCultureArchiveFileName);
			return false;
		}
	}

	return true;
}
