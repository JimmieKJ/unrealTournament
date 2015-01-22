// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// MaterialInstanceConstantFactoryNew
//=============================================================================

#pragma once
#include "MaterialInstanceConstantFactoryNew.generated.h"

UCLASS(hidecategories=Object, collapsecategories,MinimalAPI)
class UMaterialInstanceConstantFactoryNew : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class UMaterialInterface* InitialParent;

	// Begin UFactory Interface
	UNREALED_API virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	// Begin UFactory Interface	
};



