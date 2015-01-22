// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Abstract base class of animation composite base
 * This contains Composite Section data and some necessary interface to make this work
 */

#pragma once
#include "Animation/AnimCompositeBase.h"
#include "Animation/EditorAnimBaseObj.h"
#include "EditorAnimSegment.generated.h"

DECLARE_DELEGATE_OneParam( FOnAnimSegmentChanged, class UEditorAnimSegment*)


UCLASS(hidecategories=UObject, MinimalAPI, BlueprintType)
class UEditorAnimSegment: public UEditorAnimBaseObj
{
	GENERATED_UCLASS_BODY()
public:

	/** Default blend in time. */
	UPROPERTY(EditAnywhere, Category=Montage)
	FAnimSegment AnimSegment;

	int AnimSlotIndex;
	int AnimSegmentIndex;

	virtual void InitAnimSegment(int AnimSlotIndex, int AnimSegmentIndex);
	virtual bool ApplyChangesToMontage() override;

	virtual bool PropertyChangeRequiresRebuild(FPropertyChangedEvent& PropertyChangedEvent) override;
};
