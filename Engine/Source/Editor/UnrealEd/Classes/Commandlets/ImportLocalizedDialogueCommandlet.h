// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Commandlets/GatherTextCommandletBase.h"
#include "ImportLocalizedDialogueCommandlet.generated.h"

class USoundWave;
class USoundFactory;
class FInternationalizationManifest;
class FInternationalizationArchive;

/**
 *	UImportLocalizedDialogueCommandlet: Handles synchronizing localized raw audio files with dialogue and sound wave assets.
 */
UCLASS()
class UImportLocalizedDialogueCommandlet : public UGatherTextCommandletBase
{
    GENERATED_UCLASS_BODY()

public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

private:
	struct FCultureImportInfo
	{
		FString Name;
		FString AudioPath;
		FString ArchiveFileName;
		FString LocalizedRootContentPath;
		FString LocalizedRootPackagePath;
		FString LocalizedImportedDialoguePackagePath;
		bool bIsNativeCulture;
	};

	bool ImportDialogueForCulture(FLocTextHelper& InLocTextHelper, UDialogueWave* const DialogueWave, const FString& DialogueWaveSubPath, const FCultureImportInfo& InCultureImportInfo, const bool bImportAsSource);

	USoundWave* ConditionalImportSoundWave(const FString& InSoundWavePackageName, const FString& InSoundWaveAssetName, const FString& InWavFilename) const;
	USoundWave* ImportSoundWave(const FString& InSoundWavePackageName, const FString& InSoundWaveAssetName, const FString& InWavFilename) const;

	UPROPERTY()
	USoundFactory* SoundWaveFactory;

	TSet<FName> AssetsToKeep;
};
