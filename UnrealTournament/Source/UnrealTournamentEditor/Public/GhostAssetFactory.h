// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "GhostAssetFactory.generated.h"

/**
 * 
 */
UCLASS()
class UNREALTOURNAMENTEDITOR_API UGhostAssetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()
	
	// Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// Begin UFactory Interface
	
	
};
