// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CameraAnimFactory.generated.h"

UCLASS(hidecategories=Object, MinimalAPI)
class UCameraAnimFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual FName GetNewAssetThumbnailOverride() const override;
	virtual uint32 GetMenuCategories() const override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	// Begin UFactory Interface
};



