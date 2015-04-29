// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	/** life time scaling */
	UPROPERTY()
	float LifetimeScaling;
	UPROPERTY()
	float FadeMultipllier;

	FTimedImpactEffect()
	{}
	FTimedImpactEffect(USceneComponent* InComp, float InTime, float InScaling)
		: EffectComp(InComp), CreationTime(InTime), LifetimeScaling(InScaling), FadeMultipllier(1.0f)
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

UENUM()
enum ETimedLightParameter
{
	TLP_Intensity,
	TLP_Color,
};

/** used to animate any light component over time */
USTRUCT()
struct FTimedLightParameter
{
	GENERATED_USTRUCT_BODY()

	/** the light component to operator on */
	UPROPERTY()
	TWeakObjectPtr<class ULightComponent> Light;
	/** parameter to change */
	UPROPERTY()
	TEnumAsByte<ETimedLightParameter> Param;
	/** the curve to retrieve values from */
	UPROPERTY()
	UCurveBase* ParamCurve;
	/** elapsed time along the curve */
	UPROPERTY()
	float ElapsedTime;

	FTimedLightParameter()
		: Light(NULL)
	{}
	FTimedLightParameter(ULightComponent* InLight, ETimedLightParameter InParam, UCurveBase* InCurve)
		: Light(InLight), Param(InParam), ParamCurve(InCurve), ElapsedTime(0.0f)
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

	UPROPERTY()
	TArray<FTimedImpactEffect> FadingEffects;

protected:
	/** level summary for UI details */
	UPROPERTY(VisibleAnywhere, Instanced, Category = LevelSummary)
	class UUTLevelSummary* LevelSummary;
	
	virtual void CreateLevelSummary();
public:

	inline const UUTLevelSummary* GetLevelSummary() const
	{
		return LevelSummary;
	}

	/** whether to allow side switching (swap bases in CTF, etc) if the gametype wants */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameSettings)
	bool bAllowSideSwitching;

	/** level music */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = LevelSettings)
	USoundBase* Music;

	UPROPERTY()
	UAudioComponent* MusicComp;

	virtual void PostLoad() override;
	virtual void PostInitProperties() override;

	/** overridden to set bBeginPlay to true BEFORE calling BeginPlay() events, which maintains the historical behavior when an Actor spawns another from within BeginPlay(),
	  * specifically that said second Actor's BeginPlay() is called immediately as part of SpawnActor()
	  */
	virtual void NotifyBeginPlay() override;

	virtual void BeginPlay() override;
	
	/** add an impact effect that will be managed by the timing system */
	virtual void AddImpactEffect(class USceneComponent* NewEffect, float LifeScaling=1.f);

	/** checks lifetime on all active effects and culls as necessary */
	virtual void ExpireImpactEffects();

	virtual void FadeImpactEffects(float DeltaTime);

	const float ImpactEffectFadeTime;
	
	UPROPERTY()
	float ImpactEffectFadeSpeed;

	UPROPERTY()
	TArray<FTimedMaterialParameter> MaterialParamCurves;
	UPROPERTY()
	TArray<FTimedLightParameter> LightParamCurves;

	/** adds a material parameter curve to manage timing for */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = Effects)
	virtual void AddTimedMaterialParameter(UMaterialInstanceDynamic* InMI, FName InParamName, UCurveBase* InCurve, bool bInClearOnComplete = true);
	/** adds a light parameter curve to manage timing for */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = Effects)
	virtual void AddTimedLightParameter(ULightComponent* InLight, ETimedLightParameter InParam, UCurveBase* InCurve);

	virtual void Tick(float DeltaTime) override;

	/** Return true if effect is relevant and should be spawned on this machine.
	@PARAM RelevantActor - actor associated with effect being tested
	@PARAM SpawnLocation - the location at which the effect will be spawned.
	@PARAM bSpawnNearSelf - if true, the visibility of RelevantActor is relevant to the relevancy decision.
	@PARAM bIsLocallyOwnedEffect - if true, effect is instigated by player local to this client
	@PARAM CullDistance - maximum distance from nearest local viewer to spawn this effect.
	@PARAM AlwaysSpawnDistance - if closer than this to nearest local viewer, always spawn.
	@PARAM bForceDedicated - Controls whether this effect is relevant on dedicated servers. */
	virtual bool EffectIsRelevant(AActor* RelevantActor, const FVector& SpawnLocation, bool bSpawnNearSelf, bool bIsLocallyOwnedEffect, float CullDistance, float AlwaysSpawnDist, bool bForceDedicated = false);
};