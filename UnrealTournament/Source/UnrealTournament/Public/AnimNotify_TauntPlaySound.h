// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_TauntPlaySound.generated.h"

UCLASS(const, hidecategories = Object, collapsecategories, meta = (DisplayName = "Taunt Play Sound"))
class UAnimNotify_TauntPlaySound : public UAnimNotify
{
	GENERATED_BODY()

public:	
	// Begin UAnimNotify interface
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
	// End UAnimNotify interface

	// Sound to Play
	UPROPERTY(EditAnywhere, Category="AnimNotify")
	USoundBase* Sound;

};