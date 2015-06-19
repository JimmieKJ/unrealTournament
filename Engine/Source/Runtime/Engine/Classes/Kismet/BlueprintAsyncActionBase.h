// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BlueprintAsyncActionBase.generated.h"

UCLASS()
class ENGINE_API UBlueprintAsyncActionBase : public UObject
{
	GENERATED_UCLASS_BODY()

	// Called to trigger the action once the delegates have been bound
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true"))
	virtual void Activate();
};
