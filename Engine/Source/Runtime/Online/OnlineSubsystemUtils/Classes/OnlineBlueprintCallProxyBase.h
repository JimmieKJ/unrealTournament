// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "OnlineBlueprintCallProxyBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEmptyOnlineDelegate);

UCLASS(MinimalAPI)
class UOnlineBlueprintCallProxyBase : public UObject
{
	GENERATED_UCLASS_BODY()

	// Called to trigger the actual online action once the delegates have been bound
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true"), Category="Online")
	virtual void Activate();
};
