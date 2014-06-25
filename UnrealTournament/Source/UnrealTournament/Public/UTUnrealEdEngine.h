// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "UTLoadMap.h"

#include "Editor/UnrealEd/Classes/Editor/UnrealEdEngine.h"
#include "UTUnrealEdEngine.generated.h"

UCLASS(CustomConstructor)
class UUTUnrealEdEngine : public UUnrealEdEngine
{
	GENERATED_UCLASS_BODY()

	UUTUnrealEdEngine(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	UT_LOADMAP_DEFINITION()
};

#endif