// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphUtilities.h"

class BLUEPRINTGRAPH_API FBlueprintGraphPanelPinFactory: public FGraphPanelPinFactory
{
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override;
};
