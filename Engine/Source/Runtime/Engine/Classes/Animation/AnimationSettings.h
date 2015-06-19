// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimationSettings.h: Declares the AnimationSettings class.
=============================================================================*/

#pragma once

#include "AnimSequence.h"
#include "AnimationSettings.generated.h"

/**
 * Default animation settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Animation"))
class ENGINE_API UAnimationSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	// compression upgrade version
	UPROPERTY(config, VisibleAnywhere, Category = Compression)
	int32 CompressCommandletVersion;

	UPROPERTY(config, EditAnywhere, Category = Compression)
	TArray<FString> KeyEndEffectorsMatchNameArray;

	UPROPERTY(config, EditAnywhere, Category = Compression)
	TSubclassOf<class UAnimCompress>  DefaultCompressionAlgorithm; 

	UPROPERTY(config, EditAnywhere, Category = Compression)
	TEnumAsByte<AnimationCompressionFormat> RotationCompressionFormat;

	UPROPERTY(config, EditAnywhere, Category = Compression)
	TEnumAsByte<AnimationCompressionFormat> TranslationCompressionFormat;

	UPROPERTY(config, EditAnywhere, Category = Compression, meta=(ClampMin = "0", UIMin = "0"))
	float AlternativeCompressionThreshold;

	UPROPERTY(config, EditAnywhere, Category = Compression)
	bool ForceRecompression;

	UPROPERTY(config, EditAnywhere, Category = Compression)
	bool bOnlyCheckForMissingSkeletalMeshes;
	
	UPROPERTY(config, EditAnywhere, Category = Compression)
	bool bForceBelowThreshold;

	UPROPERTY(config, EditAnywhere, Category = Compression)
	bool bFirstRecompressUsingCurrentOrDefault;

	UPROPERTY(config, EditAnywhere, Category = Compression)
	bool bRaiseMaxErrorToExisting;

	UPROPERTY(config, EditAnywhere, Category = Compression)
	bool bTryFixedBitwiseCompression;

	UPROPERTY(config, EditAnywhere, Category = Compression)
	bool bTryPerTrackBitwiseCompression;

	UPROPERTY(config, EditAnywhere, Category = Compression)
	bool bTryLinearKeyRemovalCompression;

	UPROPERTY(config, EditAnywhere, Category = Compression)
	bool bTryIntervalKeyRemoval;

public:
	static UAnimationSettings * Get() { return CastChecked<UAnimationSettings>(UAnimationSettings::StaticClass()->GetDefaultObject()); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};
