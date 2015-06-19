// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 * Context to sound wave map for spoken dialogue 
 */

#include "DialogueTypes.h"

#include "DialogueWave.generated.h"

class USoundWave;
class UDialogueSoundWaveProxy;

USTRUCT()
struct FDialogueContextMapping
{
	GENERATED_USTRUCT_BODY()

	FDialogueContextMapping();

	/* The context of the dialogue. */
	UPROPERTY(EditAnywhere, Category=DialogueContextMapping )
	FDialogueContext Context;

	/* The soundwave to play for this dialogue. */
	UPROPERTY(EditAnywhere, Category=DialogueContextMapping )
	USoundWave* SoundWave;

	/* Cached object for playing the soundwave with subtitle information included. */
	UPROPERTY(Transient)
	UDialogueSoundWaveProxy* Proxy;
};

bool operator==(const FDialogueContextMapping& LHS, const FDialogueContextMapping& RHS);
bool operator!=(const FDialogueContextMapping& LHS, const FDialogueContextMapping& RHS);

UCLASS(hidecategories=Object, editinlinenew, MinimalAPI, BlueprintType)
class UDialogueWave : public UObject
{
	GENERATED_UCLASS_BODY()

	/** true if this dialogue is considered to contain mature/adult content. */
	UPROPERTY(EditAnywhere, Category=Filter, AssetRegistrySearchable)
	uint32 bMature:1;

	/** A localized version of the text that is actually spoken phonetically in the audio. */
	UPROPERTY(EditAnywhere, Category=Script )
	FString SpokenText;

#if WITH_EDITORONLY_DATA
	/** Provides contextual information for the sound to the translator - Notes to the voice actor intended to direct their performance. */
	UPROPERTY(EditAnywhere, Category=Script )
	FString VoiceActorDirection;
#endif // WITH_EDITORONLY_DATA

	/* Mappings between dialogue contexts and associated soundwaves. */
	UPROPERTY(EditAnywhere, Category=DialogueContexts )
	TArray<FDialogueContextMapping> ContextMappings;

	UPROPERTY()
	FGuid LocalizationGUID;

public:
	// Begin UObject interface. 
	virtual void Serialize( FArchive& Ar ) override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual FString GetDesc() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	// End UObject interface. 

	// Begin UDialogueWave interface.
	ENGINE_API bool SupportsContext(const FDialogueContext& Context) const;
	ENGINE_API USoundBase* GetWaveFromContext(const FDialogueContext& Context) const;
	ENGINE_API FString GetContextLocalizationKey( const FDialogueContext& Context ) const;
	// End UDialogueWave interface.

protected:
	void UpdateMappingProxy(FDialogueContextMapping& ContextMapping);
};
