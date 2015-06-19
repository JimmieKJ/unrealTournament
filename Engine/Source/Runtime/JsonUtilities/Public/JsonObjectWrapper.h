// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json.h"
#include "JsonObjectConverter.h"

#include "JsonObjectWrapper.generated.h"

/** UStruct that holds a JsonObject, can be used by structs passed to JsonObjectConverter to pass through JsonObjects directly */
USTRUCT()
struct JSONUTILITIES_API FJsonObjectWrapper
{
	GENERATED_USTRUCT_BODY()

	TSharedPtr<FJsonObject> JsonObject;
};