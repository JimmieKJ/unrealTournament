// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoolBinding.generated.h"

UCLASS()
class UMG_API UBoolBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UBoolBinding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	UFUNCTION()
	bool GetValue() const;
};
