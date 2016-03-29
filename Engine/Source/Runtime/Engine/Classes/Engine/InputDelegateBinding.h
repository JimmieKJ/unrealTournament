// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/DynamicBlueprintBinding.h"
#include "InputDelegateBinding.generated.h"

USTRUCT()
struct ENGINE_API FBlueprintInputDelegateBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint32 bConsumeInput:1;

	UPROPERTY()
	uint32 bExecuteWhenPaused:1;

	UPROPERTY()
	uint32 bOverrideParentBinding:1;

	FBlueprintInputDelegateBinding()
		: bConsumeInput(true)
		, bExecuteWhenPaused(false)
		, bOverrideParentBinding(true)
	{
	}
};

UCLASS(abstract)
class ENGINE_API UInputDelegateBinding : public UDynamicBlueprintBinding
{
	GENERATED_UCLASS_BODY()

	virtual void BindToInputComponent(UInputComponent* InputComponent) const { };
	static bool SupportsInputDelegate(const UClass* InClass);
	static void BindInputDelegates(const UClass* InClass, UInputComponent* InputComponent);
};
