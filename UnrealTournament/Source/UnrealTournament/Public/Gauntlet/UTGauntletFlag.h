// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFFlag.h"
#include "UTFlagReturnTrail.h"
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

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Flag)
	UParticleSystemComponent* TimerEffect;

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
	
	// Rebuild tghe follow path for this holder
	virtual void OnHolderChanged();

	virtual void Destroyed();
	virtual void SendHome();

	UPROPERTY()
	bool bDebugGPS;

	UPROPERTY()
	bool bDisableGPS;


protected:

	void GenerateGPSPath();
	void ValidateGPSPath();

	TArray<FRouteCacheItem> GPSRoute;					

	virtual void OnObjectStateChanged();
	bool bIgnoreClearGhostCalls;

	UPROPERTY()
	AUTRecastNavMesh* NavData;

	UPROPERTY()
	class AUTFlagReturnTrail* Trail;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AUTFlagReturnTrail> TrailClass;

	void CleanupTrail();

	bool PlaceExtraPoint(const FVector& A, const FVector& B, FVector& ExtraPoint, int32 Step);
	void MoveToFloor(FVector& PointLocation);

};