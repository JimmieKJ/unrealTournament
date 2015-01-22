// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AI/Navigation/NavAreas/NavArea.h"
#include "NavArea_Default.generated.h"

/** Regular navigation area, applied to entire navigation data by default */
UCLASS(Config=Engine)
class ENGINE_API UNavArea_Default : public UNavArea
{
	GENERATED_UCLASS_BODY()
};
