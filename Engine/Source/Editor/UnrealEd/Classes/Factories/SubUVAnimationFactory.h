// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
* Factory for SubUVAnimation assets
*/

#pragma once

#include "SubUVAnimationFactory.generated.h"


UCLASS(MinimalAPI)
class USubUVAnimationFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	/** An initial texture to use */
	UPROPERTY()
	class UTexture2D* InitialTexture;

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const;
	// End of UFactory interface
};
