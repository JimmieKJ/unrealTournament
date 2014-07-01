// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLoadMap.h"

#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#include "UTEditorEngine.generated.h"

UCLASS(CustomConstructor)
class UUTEditorEngine : public UEditorEngine
{
	GENERATED_UCLASS_BODY()

	UUTEditorEngine(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	UT_LOADMAP_DEFINITION()
};
