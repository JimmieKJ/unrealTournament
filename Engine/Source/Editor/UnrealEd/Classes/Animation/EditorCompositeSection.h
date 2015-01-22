// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Abstract base class of animation composite base
 * This contains Composite Section data and some necessary interface to make this work
 */

#pragma once
#include "Animation/AnimMontage.h"
#include "Animation/EditorAnimBaseObj.h"
#include "EditorCompositeSection.generated.h"

UCLASS(hidecategories=UObject, MinimalAPI, BlueprintType)
class UEditorCompositeSection: public UEditorAnimBaseObj
{
	GENERATED_UCLASS_BODY()
public:

	/** Default blend in time. */
	UPROPERTY(EditAnywhere, Category=Montage)
	FCompositeSection CompositeSection;

	int32 SectionIndex;
	
	virtual void InitSection(int SectionIndex);
	virtual bool ApplyChangesToMontage() override;
};
