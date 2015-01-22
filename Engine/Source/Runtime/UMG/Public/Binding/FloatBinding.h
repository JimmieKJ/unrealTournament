// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FloatBinding.generated.h"

UCLASS()
class UMG_API UFloatBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UFloatBinding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	UFUNCTION()
	float GetValue() const;
};
