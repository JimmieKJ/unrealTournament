// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "BlueprintUtilities.h"
#include "Engine/InputAxisDelegateBinding.h"

UInputAxisDelegateBinding::UInputAxisDelegateBinding(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInputAxisDelegateBinding::BindToInputComponent(UInputComponent* InputComponent) const
{
	TArray<FInputAxisBinding> BindsToAdd;

	for (int32 BindIndex=0; BindIndex<InputAxisDelegateBindings.Num(); ++BindIndex)
	{
		const FBlueprintInputAxisDelegateBinding& Binding = InputAxisDelegateBindings[BindIndex];

		FInputAxisBinding AB( Binding.InputAxisName );
		AB.bConsumeInput = Binding.bConsumeInput;
		AB.bExecuteWhenPaused = Binding.bExecuteWhenPaused;
		AB.AxisDelegate.BindDelegate(InputComponent->GetOwner(), Binding.FunctionNameToBind);

		if (Binding.bOverrideParentBinding)
		{
			for (int32 ExistingIndex = InputComponent->AxisBindings.Num() - 1; ExistingIndex >= 0; --ExistingIndex)
			{
				const FInputAxisBinding& ExistingBind = InputComponent->AxisBindings[ExistingIndex];
				if (ExistingBind.AxisName == AB.AxisName)
				{
					InputComponent->AxisBindings.RemoveAt(ExistingIndex);
				}
			}
		}

		// To avoid binds in the same layer being removed by the parent override temporarily put them in this array and add later
		BindsToAdd.Add(AB);
	}

	for (int32 Index=0; Index < BindsToAdd.Num(); ++Index)
	{
		InputComponent->AxisBindings.Add(BindsToAdd[Index]);
	}
}