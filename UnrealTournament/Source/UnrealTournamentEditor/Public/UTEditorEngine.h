// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLoadMap.h"

#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#include "UTEditorEngine.generated.h"

UCLASS(CustomConstructor)
class UUTEditorEngine : public UEditorEngine
{
	GENERATED_UCLASS_BODY()

	UUTEditorEngine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	UT_LOADMAP_DEFINITION()
};
