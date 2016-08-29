// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// PhysicalMaterialFactoryNew
//~=============================================================================

#pragma once
#include "PhysicalMaterialFactoryNew.generated.h"

UCLASS(MinimalAPI, HideCategories=Object)
class UPhysicalMaterialFactoryNew : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = PhysicalMaterialFactory)
	TSubclassOf<UPhysicalMaterial> PhysicalMaterialClass;

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	
};
