// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/InputAxisKeyDelegateBinding.h"
#include "InputVectorAxisDelegateBinding.generated.h"

UCLASS()
class ENGINE_API UInputVectorAxisDelegateBinding : public UInputAxisKeyDelegateBinding
{
	GENERATED_UCLASS_BODY()

	//~ Begin UInputDelegateBinding Interface
	virtual void BindToInputComponent(UInputComponent* InputComponent) const override;
	//~ End UInputDelegateBinding Interface
};
