// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AIDataProvider.h"
#include "AIDataProvider_QueryParams.generated.h"

/**
 * AIDataProvider_QueryParams is used with environment queries
 *
 * It allows defining simple parameters for running query,
 * which are not tied to any specific pawn, but defined
 * for every query execution.
 */

UCLASS(EditInlineNew, meta=(DisplayName="Query Params"))
class AIMODULE_API UAIDataProvider_QueryParams : public UAIDataProvider
{
	GENERATED_UCLASS_BODY()

	virtual void BindData(UObject* Owner, int32 RequestId) override;
	virtual FString ToString(FName PropName) const override;

	UPROPERTY(EditAnywhere, Category = Provider)
	FName ParamName;

	UPROPERTY()
	float FloatValue;

	UPROPERTY()
	int32 IntValue;

	UPROPERTY()
	bool BoolValue;
};
