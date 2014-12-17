// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeapon.h"

#include "UTWeap_LinkGun.generated.h"

UCLASS(Abstract)
class UNREALTOURNAMENT_API AUTWeap_LinkGun : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadOnly, Category = Bio)
	class AUTProj_BioShot* LinkedBio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = LinkGun)
	int32 Links;

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


	UFUNCTION(exec)
	virtual void DebugSetLinkGunLinks(int32 newLinks);


	// override to handle setting Link Bolt properties by Links.
	virtual AUTProjectile* FireProjectile() override;

	// override to handle link beam distance scaling by Links
	virtual void FireInstantHit(bool bDealDamage = true, FHitResult* OutHit = NULL) override;

	// Overridden to call linkedConsumeAmmo on gun that's linked to us
	virtual void ConsumeAmmo(uint8 FireModeNum);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual void LinkedConsumeAmmo(int32 Mode);

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated, Category = LinkGun)
	AActor* LinkTarget;

	// sound made when link is established to another player's gun (played from LinkedCharacter)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	USoundBase* LinkEstablishedOtherSound;

	// sound made when link is established another player (played from self)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	USoundBase* LinkEstablishedSelfSound;


	// default link ambient sound volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	uint8    LinkVolume;

	// final volume to play link ambient sound
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	uint8    SentLinkVolume;

	//
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LinkGun)
	bool bDoHit;

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

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual bool AddLink(int32 Size, AUTCharacter* Starter);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual void RemoveLink(int32 Size, AUTCharacter* Starter);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual void ClearLinksTo();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual void ClearLinksFrom();

	virtual void PlayImpactEffects(const FVector& TargetLoc, uint8 FireMode, const FVector& SpawnLocation, const FRotator& SpawnRotation) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = LinkGun)
	virtual void ClearLinks();

	virtual void StopFire(uint8 FireModeNum) override;

	virtual void ServerStopFire_Implementation(uint8 FireModeNum) override;

	// reset links
	virtual void OnBringUp();
    
	// reset links
	virtual bool PutDown() override;
};
