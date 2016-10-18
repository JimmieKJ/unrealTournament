// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 *	This will hold all of our enums and types and such that we need to
 *	use in multiple files where the enum can't be mapped to a specific file.
 */

#include "DialogueTypes.generated.h"

UENUM()
namespace EGrammaticalGender
{
	enum Type
	{
		Neuter		UMETA( DisplayName = "Neuter" ),
		Masculine	UMETA( DisplayName = "Masculine" ),
		Feminine	UMETA( DisplayName = "Feminine" ),
		Mixed		UMETA( DisplayName = "Mixed" ),
	};
}

UENUM()
namespace EGrammaticalNumber
{
	enum Type
	{
		Singular	UMETA( DisplayName = "Singular" ),
		Plural		UMETA( DisplayName = "Plural" ),
	};
}

class UDialogueVoice;
class UDialogueWave;

USTRUCT(BlueprintType)
struct ENGINE_API FDialogueContext
{
	GENERATED_USTRUCT_BODY()

	FDialogueContext();

	/** The person speaking the dialogue. */
	UPROPERTY(EditAnywhere, Category=DialogueContext )
	UDialogueVoice* Speaker;

	/** The people being spoken to. */
	UPROPERTY(EditAnywhere, Category=DialogueContext )
	TArray<UDialogueVoice*> Targets;

	/** Gets a generated hash created from the source and targets. */
	FString GetContextHash() const;
};

ENGINE_API bool operator==(const FDialogueContext& LHS, const FDialogueContext& RHS);
ENGINE_API bool operator!=(const FDialogueContext& LHS, const FDialogueContext& RHS);

USTRUCT()
struct ENGINE_API FDialogueWaveParameter
{
	GENERATED_USTRUCT_BODY()

	FDialogueWaveParameter();

	/** The dialogue wave to play. */
	UPROPERTY(EditAnywhere, Category=DialogueWaveParameter )
	UDialogueWave* DialogueWave;

	/** The context to use for the dialogue wave. */
	UPROPERTY(EditAnywhere, Category=DialogueWaveParameter )
	FDialogueContext Context;
};
