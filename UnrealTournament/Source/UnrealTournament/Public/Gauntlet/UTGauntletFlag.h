// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFFlag.h"
#include "UTGauntletFlag.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTGauntletFlag: public AUTCTFFlag
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
	virtual void PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir);

	virtual void SetTeam(AUTTeamInfo* NewTeam);
	virtual void TeamReset();

	void NoLongerHeld(AController* InstigatedBy);

	// Returns a status message for this object on the hud.
	virtual FText GetHUDStatusMessage(AUTHUD* HUD);

	UPROPERTY(Replicated)
	bool bPendingTeamSwitch;

	virtual void ClearGhostFlags();

protected:
	virtual void OnObjectStateChanged();
	bool bIgnoreClearGhostCalls;



};