// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CurveBase.h"
#include "CurveFloat.generated.h"

USTRUCT()
struct ENGINE_API FRuntimeFloatCurve
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FRichCurve EditorCurveData;

	UPROPERTY(EditAnywhere,Category=RuntimeFloatCurve)
	class UCurveFloat* ExternalCurve;

	FRuntimeFloatCurve();

	/** Get the current curve struct */
	FRichCurve* GetRichCurve();
	const FRichCurve* GetRichCurveConst() const;
};

UCLASS(BlueprintType, MinimalAPI)
class UCurveFloat : public UCurveBase
{
	GENERATED_UCLASS_BODY()

	/** Keyframe data */
	UPROPERTY()
	FRichCurve FloatCurve;

	/** Flag to represent event curve */
	UPROPERTY()
	bool	bIsEventCurve;

	/** Evaluate this float curve at the specified time */
	UFUNCTION(BlueprintCallable, Category="Math|Curves")
	ENGINE_API float GetFloatValue(float InTime) const;

	// Begin FCurveOwnerInterface
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;

	/** Determine if Curve is the same */
	ENGINE_API bool operator == (const UCurveFloat& Curve) const;
};

