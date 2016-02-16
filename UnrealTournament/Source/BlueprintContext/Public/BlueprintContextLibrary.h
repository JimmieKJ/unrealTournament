// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Blueprint/UserWidget.h"

#include "BlueprintContextLibrary.generated.h"

UCLASS()
class BLUEPRINTCONTEXT_API UBlueprintContextLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION( BlueprintPure, BlueprintCosmetic, Category = BlueprintContext,
	           meta = ( WorldContext = "ContextObject", BlueprintInternalUseOnly = "true" ) )
	static UBlueprintContextBase* GetContext( UObject* ContextObject, TSubclassOf< UBlueprintContextBase > Class );

	static UWorld* GetWorldFrom( UObject* ContextObject );
};