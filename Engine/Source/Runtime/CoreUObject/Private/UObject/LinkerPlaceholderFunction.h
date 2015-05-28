// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Class.h" // for UFunction
#include "LinkerPlaceholderBase.h"

// Forward declarations
class FObjectInitializer;

/**  */
class ULinkerPlaceholderFunction : public UFunction, public TLinkerImportPlaceholder<UFunction>
{
public:
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(ULinkerPlaceholderFunction, UFunction, /*TStaticFlags =*/0, CoreUObject, /*TStaticCastFlags =*/0, NO_API)

	ULinkerPlaceholderFunction(const FObjectInitializer& ObjectInitializer);

	// FLinkerPlaceholderBase interface 
	virtual UObject* GetPlaceholderAsUObject() override { return this; }
	// End of FLinkerPlaceholderBase interface
};
