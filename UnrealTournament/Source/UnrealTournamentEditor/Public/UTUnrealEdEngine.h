// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLoadMap.h"

#include "Editor/UnrealEd/Classes/Editor/UnrealEdEngine.h"
#include "UTWorldSettings.h"
#include "UTMenuGameMode.h"

#include "UTUnrealEdEngine.generated.h"

UCLASS(CustomConstructor)
class UUTUnrealEdEngine : public UUnrealEdEngine
{
	GENERATED_UCLASS_BODY()

	UUTUnrealEdEngine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	UT_LOADMAP_DEFINITION()
};
