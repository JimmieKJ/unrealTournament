// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "BlueprintUtilities.h"
#include "Engine/InputDelegateBinding.h"
#include "Engine/InputActionDelegateBinding.h"
#include "Engine/InputAxisDelegateBinding.h"
#include "Engine/InputKeyDelegateBinding.h"
#include "Engine/InputTouchDelegateBinding.h"
#include "Engine/InputAxisKeyDelegateBinding.h"
#include "Engine/InputVectorAxisDelegateBinding.h"

UInputDelegateBinding::UInputDelegateBinding(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInputDelegateBinding::BindInputDelegates(const UBlueprintGeneratedClass* BGClass, UInputComponent* InputComponent)
{
	static UClass* InputBindingClasses[] = { 
												UInputActionDelegateBinding::StaticClass(), 
												UInputAxisDelegateBinding::StaticClass(), 
												UInputKeyDelegateBinding::StaticClass(),
												UInputTouchDelegateBinding::StaticClass(),
												UInputAxisKeyDelegateBinding::StaticClass(),
												UInputVectorAxisDelegateBinding::StaticClass(),
										   };

	if (BGClass)
	{
		BindInputDelegates(Cast<UBlueprintGeneratedClass>(BGClass->GetSuperStruct()), InputComponent);

		for (int32 Index = 0; Index < ARRAY_COUNT(InputBindingClasses); ++Index)
		{
			UInputDelegateBinding* BindingObject = CastChecked<UInputDelegateBinding>(BGClass->GetDynamicBindingObject(InputBindingClasses[Index]), ECastCheckedType::NullAllowed);
			if (BindingObject)
			{
				BindingObject->BindToInputComponent(InputComponent);
			}
		}
	}
}