// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// FontFactory: Creates a Font Factory
//=============================================================================

#pragma once
#include "FontFileImportFactory.generated.h"

UCLASS()
class UNREALED_API UFontFileImportFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// Begin UFactory Interface
	virtual UObject* FactoryCreateBinary(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn) override;
	// Begin UFactory Interface	
};



