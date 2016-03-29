// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Components/InputComponent.h"
#include "Engine/InputDelegateBinding.h"
#include "InputKeyDelegateBinding.generated.h"

USTRUCT()
struct ENGINE_API FBlueprintInputKeyDelegateBinding : public FBlueprintInputDelegateBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FInputChord InputChord;

	UPROPERTY()
	TEnumAsByte<EInputEvent> InputKeyEvent;

	UPROPERTY()
	FName FunctionNameToBind;

	FBlueprintInputKeyDelegateBinding()
		: FBlueprintInputDelegateBinding()
		, InputKeyEvent(IE_Pressed)
		, FunctionNameToBind(NAME_None)
	{
	}
};

UCLASS()
class ENGINE_API UInputKeyDelegateBinding : public UInputDelegateBinding
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FBlueprintInputKeyDelegateBinding> InputKeyDelegateBindings;

	//~ Begin UInputDelegateBinding Interface
	virtual void BindToInputComponent(UInputComponent* InputComponent) const override;
	//~ End UInputDelegateBinding Interface
};
