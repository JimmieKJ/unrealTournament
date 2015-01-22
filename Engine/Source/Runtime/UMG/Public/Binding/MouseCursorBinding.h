// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MouseCursorBinding.generated.h"

UCLASS()
class UMG_API UMouseCursorBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UMouseCursorBinding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	UFUNCTION()
	EMouseCursor::Type GetValue() const;
};
