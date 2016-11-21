// this class is simply used to trick the navmesh building into splitting up areas we want to isolate so they have their own polys (e.g. jump pad trigger zone)
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AI/Navigation/NavAreas/NavArea_Default.h"

#include "UTNavArea_Default.generated.h"

UCLASS()
class UUTNavArea_Default : public UNavArea_Default
{
	GENERATED_BODY()
public:
};