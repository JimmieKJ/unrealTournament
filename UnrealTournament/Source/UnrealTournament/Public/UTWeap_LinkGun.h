// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTWeapon.h"
#include "UTWeap_LinkGun.generated.h"

UCLASS(Abstract)
class UNREALTOURNAMENT_API AUTWeap_LinkGun : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	// every this many seconds the user can use primary while holding down alt to fire a pulse that pulls the current target
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	float BeamPulseInterval;
	// extra ammo used for pulse
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	int32 BeamPulseAmmoCost;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	float BeamPulseMomentum;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	TSubclassOf<UDamageType> BeamPulseDamageType;
	// weapon anim for pulse
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	UAnimMontage* PulseAnim;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	UAnimMontage* PulseAnimHands;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	UParticleSystem* PulseSuccessEffect;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	UParticleSystem* PulseFailEffect;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	int32 LinkPullDamage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	FLinearColor ReadyToPullColor;

	UPROPERTY(BlueprintReadWrite, Category = LinkGun)
		int32 LastFiredPlasmaTime;

	UPROPERTY(BlueprintReadOnly, Category = LinkGun)
		AActor* CurrentLinkedTarget;

	UPROPERTY(BlueprintReadOnly, Category = LinkGun)
		float LinkStartTime;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = LinkGun)
		float PullWarmupTime;

	UPROPERTY(BlueprintReadOnly, Category = LinkGun)
		bool bReadyToPull;

	UFUNCTION(Unreliable, Server, WithValidation)
	void ServerSetPulseTarget(AActor* InTarget);

	UPROPERTY(BlueprintReadWrite, Category = LinkGun)
	float OverheatFactor;

	UPROPERTY(BlueprintReadWrite, Category = LinkGun)
		bool bIsInCoolDown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
		USoundBase* OverheatFPFireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
		USoundBase* NormalFPFireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
		UAnimMontage* OverheatAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
		USoundBase* OverheatSound;

	/** jump loop anim to this section when stopping overheat anim */
	UPROPERTY(EditDefaultsOnly, Category = Effects)
		FName EndOverheatAnimSection;

	/** kickback when firing beam*/
	UPROPERTY(EditDefaultsOnly, Category = Effects)
		float FiringBeamKickbackY;

	/** kickback when link pulling*/
	UPROPERTY(EditDefaultsOnly, Category = Effects)
		float LinkPullKickbackY;

	// last time pulse was used
	UPROPERTY(Transient, BlueprintReadWrite, Category = LinkGun)
	float LastBeamPulseTime;

	/** Target being link pulled. */
	UPROPERTY(BlueprintReadOnly)
	AActor* PulseTarget;

	/** Location of beam end for link pull attempt. */
	UPROPERTY()
		FVector PulseLoc;
	
	UPROPERTY(EditDefaultsOnly, Category = LinkGUn)
		FVector MissedPulseOffset;

	/** Return true if currently in Link Pulse. */
	virtual bool IsLinkPulsing();

	virtual void FireShot() override;

	// override to handle setting Link Bolt properties by Links.
	virtual AUTProjectile* FireProjectile() override;

	/** render target for on-mesh display screen */
	UPROPERTY(BlueprintReadWrite, Category = Mesh)
	UCanvasRenderTarget2D* ScreenTexture;
	/** drawn to the display screen for a short time after holder kills an enemy */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Mesh)
	UTexture2D* ScreenKillNotifyTexture;
	/** font for text on the display screen */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Mesh)
	UFont* ScreenFont;
	/** material ID for the screen */
	UPROPERTY(EditDefaultsOnly, Category = Mesh)
	int32 ScreenMaterialID;
	/** material ID for the side screen */
	UPROPERTY(EditDefaultsOnly, Category = Mesh)
		int32 SideScreenMaterialID;
	/** material instance showing the screen */
	UPROPERTY(BlueprintReadWrite, Category = Mesh)
	UMaterialInstanceDynamic* ScreenMI;
	/** material instance showing the side screen */
	UPROPERTY(BlueprintReadWrite, Category = Mesh)
		UMaterialInstanceDynamic* SideScreenMI;

	virtual UAnimMontage* GetFiringAnim(uint8 FireMode, bool bOnHands = false) const override;

	virtual void AttachToOwner_Implementation() override;

	UFUNCTION()
	virtual void UpdateScreenTexture(UCanvas* C, int32 Width, int32 Height);

	TSharedPtr<FCanvasWordWrapper> WordWrapper;

	/** set (client only) to last time user recorded a kill while holding this weapon, used to change on-mesh screen display */
	UPROPERTY(BlueprintReadWrite, Category = Mesh)
	float LastClientKillTime;

	virtual void NotifyKillWhileHolding_Implementation(TSubclassOf<UDamageType> DmgType) override
	{
		LastClientKillTime = GetWorld()->TimeSeconds;
	}

	/** True if link beam is currently causing damage. */
	UPROPERTY(BlueprintReadOnly, Category = LinkGun)
		bool bLinkCausingDamage;

	/** True if link beam is currently hitting something. */
	UPROPERTY(BlueprintReadOnly, Category = LinkGun)
		bool bLinkBeamImpacting;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
		USoundBase* PullSucceeded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
		USoundBase* PullFailed;

	virtual void StartLinkPull();

	virtual void Tick(float DeltaTime) override;

	virtual void FiringExtraUpdated_Implementation(uint8 NewFlashExtra, uint8 InFireMode) override;

	virtual void StateChanged() override;

	virtual void PlayWeaponAnim(UAnimMontage* WeaponAnim, UAnimMontage* HandsAnim = NULL, float RateOverride = 0.0f) override;

	/** check if bot should use pulse while firing beam */
	UFUNCTION()
	virtual void CheckBotPulseFire();

	virtual float SuggestAttackStyle_Implementation() override
	{
		return 0.8;
	}
	virtual float SuggestDefenseStyle_Implementation() override
	{
		return -0.4;
	}

	virtual void DrawWeaponCrosshair_Implementation(UUTHUDWidget* WeaponHudWidget, float RenderDelta) override;
};
