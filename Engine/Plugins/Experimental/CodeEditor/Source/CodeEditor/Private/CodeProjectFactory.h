// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CodeProjectFactory.generated.h"

UCLASS()
class UCodeProjectFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface
};
