// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphSchema_K2.h"
#include "WidgetGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UWidgetGraphSchema : public UEdGraphSchema_K2
{
	GENERATED_UCLASS_BODY()

	virtual bool ShouldAlwaysPurgeOnModification() const override { return true; }

	UPROPERTY()
	FName NAME_NeverAsPin;

	UPROPERTY()
	FName NAME_PinHiddenByDefault;

	UPROPERTY()
	FName NAME_PinShownByDefault;

	UPROPERTY()
	FName NAME_AlwaysAsPin;

	UPROPERTY()
	FName NAME_OnEvaluate;
	
	UPROPERTY()
	FName DefaultEvaluationHandlerName;
	
	// TODO UMG - Add more things specific to the needs of the widget blueprint graph.
};