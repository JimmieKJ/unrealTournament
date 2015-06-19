// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "RecastFilter_UseDefaultArea.generated.h"

/** Regular navigation area, applied to entire navigation data by default */
UCLASS(MinimalAPI)
class URecastFilter_UseDefaultArea : public UNavigationQueryFilter
{
	GENERATED_UCLASS_BODY()

	virtual void InitializeFilter(const ANavigationData& NavData, FNavigationQueryFilter& Filter) const override;
};
