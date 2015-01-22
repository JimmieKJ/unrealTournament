// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// UAimOffsetBlendSpaceFactory1D
//=============================================================================

#pragma once
#include "AimOffsetBlendSpaceFactory1D.generated.h"

UCLASS(hidecategories=Object, MinimalAPI)
class UAimOffsetBlendSpaceFactory1D : public UBlendSpaceFactory1D
{
	GENERATED_UCLASS_BODY()

	// Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	// Begin UFactory Interface
};



