// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 *
 */

#pragma once
#include "CurveFactory.generated.h"

/** Enum indicating the type of curve to make */
UENUM()
enum ECurveType
{
	/** Float keyframed */
	ECT_FloatKeyframed,
	/** Vector keyframed */
	ECT_VectorKeyframed,
	ECT_MAX,
};

UCLASS(hidecategories=Object)
class UCurveFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	/** The type of blueprint that will be created */
	UPROPERTY(EditAnywhere, Category=CurveFactory)
	TSubclassOf<UCurveBase> CurveClass;

	// Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	// Begin UFactory Interface
};



