// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// SoundClassFactory
//~=============================================================================

#pragma once
#include "SoundClassFactory.generated.h"

UCLASS(MinimalAPI, hidecategories=Object)
class USoundClassFactory : public UFactory
{
	GENERATED_UCLASS_BODY()


	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	
};
