// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Binding/PropertyBinding.h"
#include "Int32Binding.generated.h"

UCLASS()
class UMG_API UInt32Binding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UInt32Binding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	UFUNCTION()
	int32 GetValue() const;
};
