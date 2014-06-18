// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLoadMap.h"

#include "UTGameEngine.generated.h"

UCLASS(CustomConstructor)
class UUTGameEngine : public UGameEngine
{
	GENERATED_UCLASS_BODY()

	UUTGameEngine(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	UT_LOADMAP_DEFINITION()
};