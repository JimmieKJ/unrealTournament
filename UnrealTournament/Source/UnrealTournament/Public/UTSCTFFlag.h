// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFFlag.h"
#include "UTSCTFFlag.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTSCTFFlag: public AUTCTFFlag
{
	GENERATED_UCLASS_BODY()

public:

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

	// Returns a status message for this object on the hud.
	virtual FText GetHUDStatusMessage(AUTHUD* HUD);

	UPROPERTY(Replicated)
	bool bPendingTeamSwitch;

protected:
	virtual void OnObjectStateChanged();


};