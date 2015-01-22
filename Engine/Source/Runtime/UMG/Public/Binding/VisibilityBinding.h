// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateWrapperTypes.h"

#include "VisibilityBinding.generated.h"

UCLASS()
class UMG_API UVisibilityBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UVisibilityBinding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	UFUNCTION()
	ESlateVisibility GetValue() const;
};
