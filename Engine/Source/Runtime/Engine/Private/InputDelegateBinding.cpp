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

bool UInputDelegateBinding::SupportsInputDelegate(const UClass* InClass)
{
	auto DynamicClass = Cast<UDynamicClass>(InClass);
	return InClass && (
		(DynamicClass && DynamicClass->DynamicBindingObjects.Num())
		|| Cast<UBlueprintGeneratedClass>(InClass)
		|| SupportsInputDelegate(InClass->GetSuperClass()));
}

void UInputDelegateBinding::BindInputDelegates(const UClass* InClass, UInputComponent* InputComponent)
{
	static UClass* InputBindingClasses[] = { 
												UInputActionDelegateBinding::StaticClass(), 
												UInputAxisDelegateBinding::StaticClass(), 
												UInputKeyDelegateBinding::StaticClass(),
												UInputTouchDelegateBinding::StaticClass(),
												UInputAxisKeyDelegateBinding::StaticClass(),
												UInputVectorAxisDelegateBinding::StaticClass(),
										   };

	if (InClass)
	{
		BindInputDelegates(InClass->GetSuperClass(), InputComponent);

		for (int32 Index = 0; Index < ARRAY_COUNT(InputBindingClasses); ++Index)
		{
			UInputDelegateBinding* BindingObject = CastChecked<UInputDelegateBinding>(
				UBlueprintGeneratedClass::GetDynamicBindingObject(InClass, InputBindingClasses[Index])
				, ECastCheckedType::NullAllowed);
			if (BindingObject)
			{
				BindingObject->BindToInputComponent(InputComponent);
			}
		}
	}
}