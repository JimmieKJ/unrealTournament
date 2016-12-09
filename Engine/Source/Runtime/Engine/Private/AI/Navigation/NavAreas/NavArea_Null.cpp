// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavAreas/NavArea_Null.h"

UNavArea_Null::UNavArea_Null(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DefaultCost = BIG_NUMBER;
	AreaFlags = 0;
}
