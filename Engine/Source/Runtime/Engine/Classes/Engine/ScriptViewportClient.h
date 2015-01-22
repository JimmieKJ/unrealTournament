// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Base class for FViewportClients that are also UObjects
 *
 */

#pragma once
#include "UnrealClient.h"
#include "ScriptViewportClient.generated.h"

UCLASS(transient)
class UScriptViewportClient : public UObject, public FCommonViewportClient
{
	GENERATED_UCLASS_BODY()

};

