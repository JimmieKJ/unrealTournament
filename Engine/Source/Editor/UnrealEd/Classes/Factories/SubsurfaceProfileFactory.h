// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SubsurfaceProfileFactory.generated.h"

UCLASS(hidecategories=Object)
class USubsurfaceProfileFactory : public UFactory
{
	GENERATED_UCLASS_BODY()


	// Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	// Begin UFactory Interface	
};



