// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWorldSettings.generated.h"

USTRUCT()
struct FTimedImpactEffect
{
	GENERATED_USTRUCT_BODY()

	/** the component */
	UPROPERTY()
	USceneComponent* EffectComp;
	/** time component was added */
	UPROPERTY()
	float CreationTime;

	FTimedImpactEffect()
	{}
	FTimedImpactEffect(USceneComponent* InComp, float InTime)
		: EffectComp(InComp), CreationTime(InTime)
	{}
};

/** used to animate a material parameter over time from any Actor */
USTRUCT()
struct FTimedMaterialParameter
{
	GENERATED_USTRUCT_BODY()

	/** the material to set parameters on */
	UPROPERTY()
	TWeakObjectPtr<class UMaterialInstanceDynamic> MI;
	/** parameter name */
	UPROPERTY()
	FName ParamName;
	/** the curve to retrieve values from */
	UPROPERTY()
	UCurveBase* ParamCurve;
	/** elapsed time along the curve */
	UPROPERTY()
	float ElapsedTime;
	/** whether to clear the parameter when the curve completes */
	UPROPERTY()
	bool bClearOnComplete;

	FTimedMaterialParameter()
		: MI(NULL)
	{}
	FTimedMaterialParameter(UMaterialInstanceDynamic* InMI, FName InParamName, UCurveBase* InCurve, bool bInClearOnComplete = true)
		: MI(InMI), ParamName(InParamName), ParamCurve(InCurve), ElapsedTime(0.0f), bClearOnComplete(bInClearOnComplete)
	{}
};

UCLASS()
class UNREALTOURNAMENT_API AUTWorldSettings : public AWorldSettings
{
	GENERATED_UCLASS_BODY()

	/** returns the world settings for K2 */
	UFUNCTION(BlueprintCallable, Category = World)
	static AUTWorldSettings* GetWorldSettings(UObject* WorldContextObject);

	/** maximum lifetime while onscreen of impact effects that don't end on their own such as decals
	 * set to zero for infinity
	 */
	UPROPERTY(globalconfig)
	float MaxImpactEffectVisibleLifetime;
	/** maximum lifetime while offscreen of impact effects that don't end on their own such as decals
	* set to zero for infinity
	*/
	UPROPERTY(globalconfig)
	float MaxImpactEffectInvisibleLifetime;

	/** array of impact effects that have a configurable maximum lifespan (like decals) */
	UPROPERTY()
	TArray<FTimedImpactEffect> TimedEffects;

	virtual void BeginPlay() override;
	
	/** add an impact effect that will be managed by the timing system */
	virtual void AddImpactEffect(class USceneComponent* NewEffect);

	/** checks lifetime on all active effects and culls as necessary */
	virtual void ExpireImpactEffects();

	UPROPERTY()
	TArray<FTimedMaterialParameter> MaterialParamCurves;

	/** adds a material parameter curve to manage timing for */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = Effects)
	virtual void AddTimedMaterialParameter(UMaterialInstanceDynamic* InMI, FName InParamName, UCurveBase* InCurve, bool bInClearOnComplete = true);

	virtual void Tick(float DeltaTime) override;
};