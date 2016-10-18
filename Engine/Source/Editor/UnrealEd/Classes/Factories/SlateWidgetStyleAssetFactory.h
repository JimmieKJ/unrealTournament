// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateWidgetStyleAssetFactory.generated.h"

/** Factory for creating SlateStyles */
UCLASS(MinimalAPI, hidecategories=Object)
class USlateWidgetStyleAssetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=StyleType)
	TSubclassOf< USlateWidgetStyleContainerBase > StyleType;

	//~ Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface
};
