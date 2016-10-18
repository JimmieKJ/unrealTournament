// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "K2Node_GetEnumeratorName.h"
#include "K2Node_GetEnumeratorNameAsString.generated.h"

UCLASS(MinimalAPI)
class UK2Node_GetEnumeratorNameAsString : public UK2Node_GetEnumeratorName
{
	GENERATED_UCLASS_BODY()

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	//~ End UK2Node Interface

	virtual FName GetFunctionName() const override;
};

