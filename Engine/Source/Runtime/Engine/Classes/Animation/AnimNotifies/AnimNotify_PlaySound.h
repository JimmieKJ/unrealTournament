// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "AnimNotify.h"
#include "AnimNotify_PlaySound.generated.h"

UCLASS(const, hidecategories=Object, collapsecategories, meta=(DisplayName="Play Sound"))
class ENGINE_API UAnimNotify_PlaySound : public UAnimNotify
{
	GENERATED_BODY()

public:

	UAnimNotify_PlaySound();

	// Begin UAnimNotify interface
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
	// End UAnimNotify interface

	// Sound to Play
	UPROPERTY(EditAnywhere, Category="AnimNotify")
	USoundBase* Sound;

	// Volume Multiplier
	UPROPERTY(EditAnywhere, Category="AnimNotify")
	float VolumeMultiplier;

	// Pitch Multiplier
	UPROPERTY(EditAnywhere, Category="AnimNotify")
	float PitchMultiplier;

	// If this sound should follow its owner
	UPROPERTY(EditAnywhere, Category = "AnimNotify")
	uint32 bFollow:1;

	// Socket or bone name to attach sound to
	UPROPERTY(EditAnywhere, Category="AnimNotify", meta=(EditCondition="bFollow"))
	FName AttachName;
};



