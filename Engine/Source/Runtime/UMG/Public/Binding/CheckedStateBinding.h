// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CheckedStateBinding.generated.h"

UCLASS()
class UMG_API UCheckedStateBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UCheckedStateBinding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	UFUNCTION()
	ECheckBoxState GetValue() const;

private:
	enum class EConversion : uint8
	{
		None,
		Bool
	};

	mutable TOptional<EConversion> bConversion;
};
