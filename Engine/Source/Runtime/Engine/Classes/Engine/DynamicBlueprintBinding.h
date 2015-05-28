// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicBlueprintBinding.generated.h"


UCLASS(abstract)
class ENGINE_API UDynamicBlueprintBinding
	: public UObject
{
	GENERATED_UCLASS_BODY()

	virtual void BindDynamicDelegates(UObject* InInstance) const { }
	virtual void UnbindDynamicDelegatesForProperty(UObject* InInstance, const UObjectProperty* InObjectProperty) const { }
};
