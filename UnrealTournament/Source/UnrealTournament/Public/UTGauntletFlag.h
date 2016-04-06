// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFFlag.h"
#include "UTGauntletFlag.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTGauntletFlag : public AUTCTFFlag
{
	GENERATED_UCLASS_BODY()

public:
	// If true, the flag has switched sides and can't be picked up by the other team.
	UPROPERTY(BlueprintReadOnly, replicated)
	bool bTeamLocked;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = GameObject)
	UMaterialInterface* NeutralMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = GameObject)
	TArray<UMaterialInterface*> TeamMaterials;

	virtual void PostInitializeComponents() override;
	virtual void SetHolder(AUTCharacter* NewHolder);
	virtual void OnRep_Team();
	virtual void MoveToHome();

	// Holds the actual amount of time before doing a team swap.  Counts down to 0.
	UPROPERTY(Replicated)
	float SwapTimer;

	void Tick(float DeltaSeconds);
	virtual void ChangeState(FName NewCarriedObjectState);
	virtual bool CanBePickedUpBy(AUTCharacter* Character);
	virtual void PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir);

	virtual void SetTeam(AUTTeamInfo* NewTeam);
	virtual void TeamReset();

	void NoLongerHeld(AController* InstigatedBy);

protected:
	virtual void OnObjectStateChanged();
	UPROPERTY(Replicated)
	bool bPendingTeamSwitch;


};