// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_GroupTauntCascade.generated.h"

UCLASS(const, hidecategories = Object, collapsecategories, meta = (DisplayName = "Group Taunt Cascade"))
class UAnimNotify_GroupTauntCascade : public UAnimNotify
{
	GENERATED_BODY()

public:
	// Begin UAnimNotify interface
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
	// End UAnimNotify interface

};