// Bots avoid these spots when moving - used for very short term stationary hazards like bio goo or sticky grenades
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTTeamInterface.h"
#include "UTBot.h"

#include "UTAvoidMarker.generated.h"

UCLASS()
class AUTAvoidMarker : public AActor, public IUTTeamInterface
{
	GENERATED_BODY()
public:

	AUTAvoidMarker(const FObjectInitializer& OI);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UCapsuleComponent* Capsule;
	UPROPERTY(BlueprintReadWrite)
	uint8 TeamNum;
	/** if true, even same team AI should try to avoid (no meaning if TeamNum == 255) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bFriendlyAvoid;

	virtual void BeginPlay() override;
	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	virtual uint8 GetTeamNum() const
	{
		return TeamNum;
	}
	void SetTeamForSideSwap_Implementation(uint8 NewTeamNum)
	{}
};