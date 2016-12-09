// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SoundSubmix.generated.h"

class USoundEffectSubmixPreset;

UCLASS()
class ENGINE_API USoundSubmix : public UObject
{
	GENERATED_UCLASS_BODY()

	// Child submixes to this sound mix
	UPROPERTY(EditAnywhere, Category = SoundSubmix)
	TArray<USoundSubmix*> ChildSubmixes;

	UPROPERTY()
	USoundSubmix* ParentSubmix;

	UPROPERTY(EditAnywhere, Category = SoundSubmix)
	TArray<USoundEffectSubmixPreset*> SubmixEffectChain;

	// The output wet level to use for the output of this submix in parent submixes
	UPROPERTY(EditAnywhere, Category = SoundSubmix)
	float OutputWetLevel;

protected:

	//~ Begin UObject Interface.
	virtual void Serialize( FArchive& Ar ) override;
	virtual FString GetDesc( void ) override;
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface.
};

