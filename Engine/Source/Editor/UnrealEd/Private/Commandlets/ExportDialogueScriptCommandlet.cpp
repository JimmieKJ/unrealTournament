// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Commandlets/ExportDialogueScriptCommandlet.h"
#include "AssetRegistryModule.h"
#include "Sound/DialogueWave.h"
#include "Sound/DialogueVoice.h"
#include "Internationalization/InternationalizationManifest.h"
#include "Internationalization/InternationalizationArchive.h"
#include "JsonInternationalizationManifestSerializer.h"
#include "JsonInternationalizationArchiveSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogExportDialogueScriptCommandlet, Log, All);

namespace
{
	struct FCollapsedDialogueContextKey
	{
		FCollapsedDialogueContextKey(const UDialogueWave* InDialogueWave, const FDialogueContextMapping* InContext, FString InLocalizedSpokenText)
			: DialogueWave(InDialogueWave)
			, Context(InContext)
			, LocalizedSpokenText(MoveTemp(InLocalizedSpokenText))
		{
		}

		bool operator==(const FCollapsedDialogueContextKey& InOther) const
		{
			// We only care about the text that is spoken, and the voice that is speaking it
			return LocalizedSpokenText.Equals(InOther.LocalizedSpokenText, ESearchCase::CaseSensitive)
				&& Context->Context.Speaker == InOther.Context->Context.Speaker;
		}

		bool operator!=(const FCollapsedDialogueContextKey& InOther) const
		{
			return !(*this == InOther);
		}

		friend inline uint32 GetTypeHash(const FCollapsedDialogueContextKey& InKey)
		{
			// We only care about the text that is spoken, and the voice that is speaking it
			uint32 KeyHash = 0;
			KeyHash = HashCombine(KeyHash, FCrc::StrCrc32(*InKey.LocalizedSpokenText)); // Need case-sensitive hash
			KeyHash = HashCombine(KeyHash, GetTypeHash(InKey.Context->Context.Speaker));
			return KeyHash;
		}

		const UDialogueWave* DialogueWave;
		const FDialogueContextMapping* Context;
		FString LocalizedSpokenText;
	};
}

UExportDialogueScriptCommandlet::UExportDialogueScriptCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UExportDialogueScriptCommandlet::Main(const FString& Params)
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
			UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("No config specified."));
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
			UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("No config section specified."));
			return -1;
		}
		SectionName = *SectionNameParamVal;
	}

	// Source path to the root folder that manifest/archive files live in
	FString SourcePath;
	if (!GetPathFromConfig(*SectionName, TEXT("SourcePath"), SourcePath, ConfigPath))
	{
		UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("No source path specified."));
		return -1;
	}

	// Destination path to the root folder that dialogue script CSV files live in
	FString DestinationPath;
	if (!GetPathFromConfig(*SectionName, TEXT("DestinationPath"), DestinationPath, ConfigPath))
	{
		UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("No destination path specified."));
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
		UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("No native culture specified."));
		return -1;
	}

	// Get cultures to generate
	TArray<FString> CulturesToGenerate;
	if (GetStringArrayFromConfig(*SectionName, TEXT("CulturesToGenerate"), CulturesToGenerate, ConfigPath) == 0)
	{
		UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("No cultures specified for import."));
		return -1;
	}

	// Get the manifest name
	FString ManifestName;
	if (!GetStringFromConfig(*SectionName, TEXT("ManifestName"), ManifestName, ConfigPath))
	{
		UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("No manifest name specified."));
		return -1;
	}

	// Get the archive name
	FString ArchiveName;
	if (!GetStringFromConfig(*SectionName, TEXT("ArchiveName"), ArchiveName, ConfigPath))
	{
		UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("No archive name specified."));
		return -1;
	}

	// Get the dialogue script name
	FString DialogueScriptName;
	if (!GetStringFromConfig(*SectionName, TEXT("DialogueScriptName"), DialogueScriptName, ConfigPath))
	{
		UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("No dialogue script name specified."));
		return -1;
	}

	// Prepare the manifest
	TSharedPtr<FInternationalizationManifest> InternationalizationManifest;
	{
		const FString ManifestFileName = SourcePath / ManifestName;
		if (!FPaths::FileExists(ManifestFileName))
		{
			UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("Failed to find manifest '%s'."), *ManifestFileName);
			return -1;
		}

		const TSharedPtr<FJsonObject> ManifestJsonObject = ReadJSONTextFile(ManifestFileName);
		if (!ManifestJsonObject.IsValid())
		{
			UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("Failed to parse manifest '%s'."), *ManifestFileName);
			return -1;
		}

		FJsonInternationalizationManifestSerializer ManifestSerializer;
		InternationalizationManifest = MakeShareable(new FInternationalizationManifest());
		if (!ManifestSerializer.DeserializeManifest(ManifestJsonObject.ToSharedRef(), InternationalizationManifest.ToSharedRef()))
		{
			UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("Failed to deserialize manifest '%s'."), *ManifestFileName);
			return -1;
		}
	}

	// Prepare the native archive
	TSharedPtr<FInternationalizationArchive> NativeArchive;
	{
		const FString NativeCulturePath = SourcePath / NativeCulture;

		const FString NativeArchiveFileName = NativeCulturePath / ArchiveName;
		if (!FPaths::FileExists(NativeArchiveFileName))
		{
			UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("Failed to find archive '%s'."), *NativeArchiveFileName);
			return -1;
		}

		const TSharedPtr<FJsonObject> ArchiveJsonObject = ReadJSONTextFile(NativeArchiveFileName);
		if (!ArchiveJsonObject.IsValid())
		{
			UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("Failed to parse archive '%s'."), *NativeArchiveFileName);
			return -1;
		}

		FJsonInternationalizationArchiveSerializer ArchiveSerializer;
		NativeArchive = MakeShareable(new FInternationalizationArchive());
		if (!ArchiveSerializer.DeserializeArchive(ArchiveJsonObject.ToSharedRef(), NativeArchive.ToSharedRef()))
		{
			UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("Failed to deserialize archive '%s'."), *NativeArchiveFileName);
			return -1;
		}
	}

	const FString RootAssetPath = FApp::HasGameName() ? TEXT("/Game") : TEXT("/Engine");

	// Prepare the asset registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.SearchAllAssets(true);

	// We want all the non-localized project specific dialogue waves
	TArray<FAssetData> AssetDataArrayForDialogueWaves;
	if (!FLocalizedAssetUtil::GetAssetsByPathAndClass(AssetRegistry, *RootAssetPath, UDialogueWave::StaticClass()->GetFName(), /*bIncludeLocalizedAssets*/false, AssetDataArrayForDialogueWaves))
	{
		UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("Unable to get dialogue wave asset data from asset registry."));
		return -1;
	}

	for (const FString& CultureName : CulturesToGenerate)
	{
		const bool bIsNativeCulture = CultureName == NativeCulture;
		const FString CultureSourcePath = SourcePath / CultureName;
		const FString CultureDestinationPath = DestinationPath / (bUseCultureDirectory ? CultureName : TEXT(""));

		// Prepare the culture archive
		TSharedPtr<FInternationalizationArchive> CultureArchive;
		if (bIsNativeCulture)
		{
			CultureArchive = NativeArchive;
		}
		else
		{
			const FString ArchiveFileName = CultureSourcePath / ArchiveName;
			if (!FPaths::FileExists(ArchiveFileName))
			{
				UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("Failed to find archive '%s'."), *ArchiveFileName);
				continue;
			}

			const TSharedPtr<FJsonObject> ArchiveJsonObject = ReadJSONTextFile(ArchiveFileName);
			if (!ArchiveJsonObject.IsValid())
			{
				UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("Failed to parse archive '%s'."), *ArchiveFileName);
				continue;
			}

			FJsonInternationalizationArchiveSerializer ArchiveSerializer;
			CultureArchive = MakeShareable(new FInternationalizationArchive());
			if (!ArchiveSerializer.DeserializeArchive(ArchiveJsonObject.ToSharedRef(), CultureArchive.ToSharedRef()))
			{
				UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("Failed to deserialize archive '%s'."), *ArchiveFileName);
				continue;
			}
		}

		TArray<TSharedPtr<FDialogueScriptEntry>> ExportedDialogueLines;
		for (const FAssetData& AssetData : AssetDataArrayForDialogueWaves)
		{
			// Verify that the found asset is a dialogue wave
			if (AssetData.GetClass() != UDialogueWave::StaticClass())
			{
				UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("Asset registry found asset '%s', but the asset with this name is not actually a dialogue wave."), *AssetData.AssetName.ToString());
				continue;
			}

			// Get the dialogue wave
			UDialogueWave* const DialogueWave = Cast<UDialogueWave>(AssetData.GetAsset());

			// Verify that the dialogue wave was loaded
			if (!DialogueWave)
			{
				UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("Asset registry found asset '%s', but the dialogue wave could not be accessed."), *AssetData.AssetName.ToString());
				continue;
			}
			
			// This maps collapsed context keys to additional contexts that were collapsed into the primary context (the context within the key) - all contexts belong to the dialogue wave in the key
			// If multiple contexts have the same speaking voice and use the same dialogue (because it translates to the same text), then only one of those contexts needs to be exported
			// The resultant audio file will create a shared asset automatically when the dialogue is imported
			TMap<FCollapsedDialogueContextKey, TArray<const FDialogueContextMapping*>> CollapsedDialogueContexts;

			// Iterate over each context to build the list of unique entries
			for (FDialogueContextMapping& ContextMapping : DialogueWave->ContextMappings)
			{
				const FString ContextLocalizationKey = DialogueWave->GetContextLocalizationKey(ContextMapping);

				// Check that this entry exists in the manifest file, as we want to skip over dialogue that we aren't gathering
				TSharedPtr<FManifestEntry> ContextManifestEntry = InternationalizationManifest->FindEntryByKey(FDialogueConstants::DialogueNamespace, ContextLocalizationKey, &DialogueWave->SpokenText);
				if (!ContextManifestEntry.IsValid())
				{
					UE_LOG(LogExportDialogueScriptCommandlet, Log, TEXT("No internationalization manifest entry was found for context '%s' in culture '%s'. This context will be skipped."), *ContextLocalizationKey, *CultureName);
					continue;
				}

				// Find the correct entry for our context
				const FContext* ContextManifestEntryContext = ContextManifestEntry->FindContextByKey(ContextLocalizationKey);
				check(ContextManifestEntryContext); // This should never fail as we pass in the key to FindEntryByKey

				// We might have a native translation for this text, in which case we need to export the translation for that text, rather than for the source text
				FString SourceText = DialogueWave->SpokenText;
				{
					TSharedPtr<FArchiveEntry> NativeArchiveEntry = NativeArchive->FindEntryBySource(FDialogueConstants::DialogueNamespace, SourceText, ContextManifestEntryContext->KeyMetadataObj);
					if (NativeArchiveEntry.IsValid())
					{
						SourceText = NativeArchiveEntry->Translation.Text;
					}
				}

				// Find the archive entry so we can export the correct text
				FString TranslatedText;
				if (bIsNativeCulture)
				{
					TranslatedText = SourceText;
				}
				else
				{
					TSharedPtr<FArchiveEntry> ArchiveEntry = CultureArchive->FindEntryBySource(FDialogueConstants::DialogueNamespace, SourceText, ContextManifestEntryContext->KeyMetadataObj);
					if (ArchiveEntry.IsValid())
					{
						TranslatedText = ArchiveEntry->Translation.Text;
					}
				}

				if (TranslatedText.IsEmpty())
				{
					UE_LOG(LogExportDialogueScriptCommandlet, Log, TEXT("Empty translation found for context '%s' in culture '%s'. This context will be skipped."), *ContextLocalizationKey, *CultureName);
					continue;
				}

				const auto CollapsedDialogueContextKey = FCollapsedDialogueContextKey(DialogueWave, &ContextMapping, TranslatedText);
				TArray<const FDialogueContextMapping*>* MergedContextsPtr = CollapsedDialogueContexts.Find(CollapsedDialogueContextKey);
				if (MergedContextsPtr)
				{
					MergedContextsPtr->Add(&ContextMapping);
				}
				else
				{
					CollapsedDialogueContexts.Add(CollapsedDialogueContextKey);
				}
			}

			// Iterate over the unique contexts and generate exported data for them
			for (const auto& CollapsedDialogueContextPair : CollapsedDialogueContexts)
			{
				TSharedRef<FDialogueScriptEntry> ExportedDialogueLine = MakeShareable(new FDialogueScriptEntry());
				PopulateDialogueScriptEntry(CollapsedDialogueContextPair.Key.DialogueWave, *CollapsedDialogueContextPair.Key.Context, CollapsedDialogueContextPair.Value, CollapsedDialogueContextPair.Key.LocalizedSpokenText, *ExportedDialogueLine);
				ExportedDialogueLines.Add(ExportedDialogueLine);
			}
		}

		// Sort the exported lines to maintain a consistent order between exports
		// Sort order is speaking voice name, then localized dialogue
		ExportedDialogueLines.Sort([](const TSharedPtr<FDialogueScriptEntry>& InFirstEntry, const TSharedPtr<FDialogueScriptEntry>& InSecondEntry) -> bool
		{
			const int32 SpeakingVoiceResult = InFirstEntry->SpeakingVoice.Compare(InSecondEntry->SpeakingVoice, ESearchCase::CaseSensitive);
			if (SpeakingVoiceResult < 0)
			{
				return true;
			}

			if (SpeakingVoiceResult == 0 && InFirstEntry->SpokenDialogue.Compare(InSecondEntry->SpokenDialogue, ESearchCase::CaseSensitive) < 0)
			{
				return true;
			}

			return false;
		});

		{
			FString CSVFileData;
			CSVFileData += GenerateCSVHeader() + TEXT("\n");
			for (const auto& DialogueScriptEntry : ExportedDialogueLines)
			{
				CSVFileData += GenerateCSVRow(*DialogueScriptEntry) + TEXT("\n");
			}

			const FString CSVFileName = CultureDestinationPath / DialogueScriptName;
			const bool bCSVFileSaved = FLocalizedAssetSCCUtil::SaveFileWithSCC(SourceControlInfo, CSVFileName, [&](const FString& InSaveFileName) -> bool
			{
				return FFileHelper::SaveStringToFile(CSVFileData, *InSaveFileName, FFileHelper::EEncodingOptions::ForceUTF8);
			});

			if (!bCSVFileSaved)
			{
				UE_LOG(LogExportDialogueScriptCommandlet, Error, TEXT("Failed to write CSV file for culture '%s' to '%s'."), *CultureName, *CSVFileName);
				continue;
			}
		}
	}

	return 0;
}

FString UExportDialogueScriptCommandlet::GenerateCSVHeader()
{
	FString CSVHeader;
	
	for (TFieldIterator<const UProperty> PropertyIt(FDialogueScriptEntry::StaticStruct(), EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); PropertyIt; ++PropertyIt)
	{
		if (!CSVHeader.IsEmpty())
		{
			CSVHeader += TEXT(",");
		}

		const FString PropertyName = PropertyIt->GetName();

		CSVHeader += TEXT("\"");
		CSVHeader += PropertyName.Replace(TEXT("\""), TEXT("\"\""));
		CSVHeader += TEXT("\"");
	}

	return CSVHeader;
}

FString UExportDialogueScriptCommandlet::GenerateCSVRow(const FDialogueScriptEntry& InDialogueScriptEntry)
{
	FString CSVRow;

	for (TFieldIterator<const UProperty> PropertyIt(FDialogueScriptEntry::StaticStruct(), EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); PropertyIt; ++PropertyIt)
	{
		if (!CSVRow.IsEmpty())
		{
			CSVRow += TEXT(",");
		}

		FString PropertyValue;
		PropertyIt->ExportText_Direct(PropertyValue, PropertyIt->ContainerPtrToValuePtr<void>(&InDialogueScriptEntry), nullptr, nullptr, PPF_None);

		CSVRow += TEXT("\"");
		CSVRow += PropertyValue.Replace(TEXT("\""), TEXT("\"\""));
		CSVRow += TEXT("\"");
	}

	return CSVRow;
}

void UExportDialogueScriptCommandlet::PopulateDialogueScriptEntry(const UDialogueWave* InDialogueWave, const FDialogueContextMapping& InPrimaryContext, const TArray<const FDialogueContextMapping*>& InAdditionalContexts, const FString& InLocalizedDialogue, FDialogueScriptEntry& OutDialogueScriptEntry)
{
	auto AppendTargetVoices = [&](const FDialogueContext& InContext)
	{
		if (InContext.Targets.Num() > 0)
		{
			FString TargetVoicesText;

			const bool bIsArray = InContext.Targets.Num() > 1;
			if (bIsArray)
			{
				TargetVoicesText += TEXT("[");
			}

			bool bIsFirst = true;
			for (const UDialogueVoice* TargetVoice : InContext.Targets)
			{
				if (!bIsFirst)
				{
					TargetVoicesText += TEXT(",");
				}
				bIsFirst = false;

				TargetVoicesText += TargetVoice->GetName();
			}

			if (bIsArray)
			{
				TargetVoicesText += TEXT("]");
			}

			OutDialogueScriptEntry.TargetVoices.Add(MoveTemp(TargetVoicesText));
		}
	};

	auto AppendTargetVoiceGUIDs = [&](const FDialogueContext& InContext)
	{
		if (InContext.Targets.Num() > 0)
		{
			FString TargetVoiceGUIDsText;

			const bool bIsArray = InContext.Targets.Num() > 1;
			if (bIsArray)
			{
				TargetVoiceGUIDsText += TEXT("[");
			}

			bool bIsFirst = true;
			for (const UDialogueVoice* TargetVoice : InContext.Targets)
			{
				if (!bIsFirst)
				{
					TargetVoiceGUIDsText += TEXT(",");
				}
				bIsFirst = false;

				TargetVoiceGUIDsText += TargetVoice->LocalizationGUID.ToString();
			}

			if (bIsArray)
			{
				TargetVoiceGUIDsText += TEXT("]");
			}

			OutDialogueScriptEntry.TargetVoiceGUIDs.Add(MoveTemp(TargetVoiceGUIDsText));
		}
	};

	OutDialogueScriptEntry.SpokenDialogue = InLocalizedDialogue;
	OutDialogueScriptEntry.VoiceActorDirection = InDialogueWave->VoiceActorDirection;
	OutDialogueScriptEntry.AudioFileName = InDialogueWave->GetContextRecordedAudioFilename(InPrimaryContext);
	OutDialogueScriptEntry.DialogueAsset = InDialogueWave->GetPathName();

	OutDialogueScriptEntry.SpeakingVoice = InPrimaryContext.Context.Speaker->GetName();
	OutDialogueScriptEntry.SpeakingVoiceGUID = InPrimaryContext.Context.Speaker->LocalizationGUID.ToString();
	OutDialogueScriptEntry.DialogueAssetGUID = InDialogueWave->LocalizationGUID.ToString();

	OutDialogueScriptEntry.LocalizationKeys.Add(InDialogueWave->GetContextLocalizationKey(InPrimaryContext));
	AppendTargetVoices(InPrimaryContext.Context);
	AppendTargetVoiceGUIDs(InPrimaryContext.Context);

	for (const FDialogueContextMapping* AdditionalContext : InAdditionalContexts)
	{
		OutDialogueScriptEntry.LocalizationKeys.Add(InDialogueWave->GetContextLocalizationKey(*AdditionalContext));
		AppendTargetVoices(AdditionalContext->Context);
		AppendTargetVoiceGUIDs(AdditionalContext->Context);
	}
}
