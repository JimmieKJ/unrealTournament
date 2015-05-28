// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetBinding.generated.h"

UCLASS()
class UMG_API UWidgetBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UWidgetBinding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	UFUNCTION()
	UWidget* GetValue() const;
};
