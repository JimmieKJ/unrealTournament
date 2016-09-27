// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTWeapon.h"
#include "UTWeap_LinkGun.generated.h"

UCLASS(Abstract)
class UNREALTOURNAMENT_API AUTWeap_LinkGun : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	// scale damage on link bolts per link
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	float PerLinkDamageScalingPrimary;

	// scale damage on link beams per link
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	float PerLinkDamageScalingSecondary;

	// scale trace range on link beams per link. Careful of 0 values here.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun, meta = (ClampMin = "0.001"))
	float PerLinkDistanceScalingSecondary;

	// link beams established to LinkTargets have range scaled by this distance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	float LinkDistanceScaling;

	// angles at which link beam to target is maintained
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	float LinkFlexibility;

	// countdown timer -- when it reaches 0 set link to new value -- @! Make Local Var?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	float LinkBreakTime;

	// time to set countdown timer to. delay between 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	float LinkBreakDelay;

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

	/** clientside pull target; server has extra leeway for hitting this in remote client scenarios to avoid pull success mismatches
	 * unused if standalone game or bot weapon
	 */
	UPROPERTY()
	AActor* ClientPulseTarget;

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

	UPROPERTY(Transient, BlueprintReadWrite, Category = LinkGun)
	bool bPendingBeamPulse;
	// last time pulse was used
	UPROPERTY(Transient, BlueprintReadWrite, Category = LinkGun)
	float LastBeamPulseTime;

	UFUNCTION(exec)
	virtual void DebugSetLinkGunLinks(int32 newLinks);

	/** Target being link pulled. */
	UPROPERTY(BlueprintReadOnly)
	AActor* PulseTarget;

	/** Location of beam end for link pull attempt. */
	UPROPERTY()
		FVector PulseLoc;

	/** Return true if currently in Link Pulse. */
	virtual bool IsLinkPulsing();

	virtual void FireShot() override;

	// override to handle setting Link Bolt properties by Links.
	virtual AUTProjectile* FireProjectile() override;

	// override to handle link beam distance scaling by Links
	virtual void FireInstantHit(bool bDealDamage = true, FHitResult* OutHit = NULL) override;

	// Overridden to call linkedConsumeAmmo on gun that's linked to us
	virtual void ConsumeAmmo(uint8 FireModeNum);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual void LinkedConsumeAmmo(int32 Mode);
	
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


protected:
	UPROPERTY(BlueprintReadOnly, Replicated, Category = LinkGun)
	AActor* LinkTarget;
	UPROPERTY(BlueprintReadOnly, Replicated, Category = LinkGun)
	int32 Links;
public:
	inline AActor* GetLinkTarget() const
	{
		return LinkTarget;
	}
	inline int32 GetNumLinks() const
	{
		return Links;
	}

	// sound made when link is established to another player's gun (played from LinkedCharacter)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	USoundBase* LinkEstablishedOtherSound;

	// sound made when link is established another player (played from self)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	USoundBase* LinkEstablishedSelfSound;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LinkGun)
	bool bFeedbackDeath;

	/** test whether another LinkGun is linked to us */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual bool LinkedTo(AUTWeap_LinkGun* L);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual bool IsLinkable(AActor* Other);

	UFUNCTION(BlueprintNativeEvent)
	bool IsLinkValid();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual void SetLinkTo(AActor* Other);

	//////////////////////////////////////////////////////////////////////////
protected:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual bool AddLink(int32 Size, AUTCharacter* Starter);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual void RemoveLink(int32 Size, AUTCharacter* Starter);
public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual void ClearLinksTo();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual void ClearLinksFrom();

	virtual void PlayImpactEffects_Implementation(const FVector& TargetLoc, uint8 FireMode, const FVector& SpawnLocation, const FRotator& SpawnRotation) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual void ClearLinks();
    
	// reset links
	virtual bool PutDown() override;

	virtual void OnMultiPress_Implementation(uint8 OtherFireMode) override;

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
