// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "CurveBase.h"
#include "CurveVector.generated.h"

UCLASS(BlueprintType, MinimalAPI)
class UCurveVector : public UCurveBase
{
	GENERATED_UCLASS_BODY()

	/** Keyframe data, one curve for X, Y and Z */
	UPROPERTY()
	FRichCurve FloatCurves[3];

	/** Evaluate this float curve at the specified time */
	UFUNCTION(BlueprintCallable, Category="Math|Curves")
	ENGINE_API FVector GetVectorValue(float InTime) const;

	// Begin FCurveOwnerInterface
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;

	/** Determine if Curve is the same */
	ENGINE_API bool operator == (const UCurveVector& Curve) const;

	virtual bool IsValidCurve( FRichCurveEditInfo CurveInfo ) override;
};

