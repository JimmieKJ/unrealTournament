// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavFilter_AIControllerDefault.generated.h"

UCLASS()
class AIMODULE_API UNavFilter_AIControllerDefault : public UNavigationQueryFilter
{
	GENERATED_BODY()
public:
	UNavFilter_AIControllerDefault(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual TSubclassOf<UNavigationQueryFilter> GetSimpleFilterForAgent(const UObject& Querier) const;
};
