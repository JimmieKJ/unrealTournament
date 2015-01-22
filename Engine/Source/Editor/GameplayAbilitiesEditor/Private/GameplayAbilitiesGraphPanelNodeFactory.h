// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphUtilities.h"
#include "SGraphNodeK2GameplayEffectVar.h"
#include "K2Node_GameplayEffectVariable.h"

class FGameplayAbilitiesGraphPanelNodeFactory: public FGraphPanelNodeFactory
{
	virtual TSharedPtr<class SGraphNode> CreateNode(class UEdGraphNode* InNode) const override
	{
		if (UK2Node_GameplayEffectVariable* GameplayEffectNode = Cast<UK2Node_GameplayEffectVariable>(InNode))
		{
			return SNew(SGraphNodeK2GameplayEffectVar, GameplayEffectNode);
		}

		return NULL;
	}
};
