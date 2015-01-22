// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TextBinding.generated.h"

UCLASS()
class UMG_API UTextBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UTextBinding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	virtual void Bind(UProperty* Property, FScriptDelegate* Delegate) override;

	UFUNCTION()
	FText GetTextValue() const;

	UFUNCTION()
	FString GetStringValue() const;

private:
	mutable TOptional<bool> bNeedsConversion;
};
