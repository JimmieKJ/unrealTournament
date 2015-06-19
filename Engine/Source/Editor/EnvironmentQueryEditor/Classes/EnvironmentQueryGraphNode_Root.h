// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "EnvironmentQueryGraphNode_Root.generated.h"

UCLASS()
class UEnvironmentQueryGraphNode_Root : public UEnvironmentQueryGraphNode
{
	GENERATED_UCLASS_BODY()

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool HasErrors() const override { return false; }
};
