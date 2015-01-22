// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// FontFactory: Creates a Font Factory
//=============================================================================

#pragma once
#include "FontFactory.generated.h"

UCLASS()
class UNREALED_API UFontFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// Begin UFactory Interface	
};



