// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicPropertyPath.h"

#include "PropertyBinding.generated.h"

UCLASS()
class UMG_API UPropertyBinding : public UObject
{
	GENERATED_BODY()

public:
	UPropertyBinding();

	virtual bool IsSupportedSource(UProperty* Property) const;
	virtual bool IsSupportedDestination(UProperty* Property) const;

	virtual void Bind(UProperty* Property, FScriptDelegate* Delegate);

public:
	/** The source object to use as the initial container to resolve the Source Property Path on. */
	UPROPERTY()
	TWeakObjectPtr<UObject> SourceObject;

	/** The property path to trace to resolve this binding on the Source Object */
	UPROPERTY()
	FDynamicPropertyPath SourcePath;

	/** Used to determine if a binding already exists on the object and if this binding can be safely removed. */
	UPROPERTY()
	FName DestinationProperty;
};
