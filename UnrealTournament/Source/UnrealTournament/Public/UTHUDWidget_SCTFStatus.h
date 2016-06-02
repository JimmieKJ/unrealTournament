// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_CTFFlagStatus.h"
#include "UTSCTFFlagBase.h"
#include "UTHUDWidget_SCTFStatus.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_SCTFStatus : public UUTHUDWidget_CTFFlagStatus
{
	GENERATED_UCLASS_BODY()

	// The text that will be displayed if you have the enemy flag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offense")
	FText OffensePrimaryMessage;

	// The text that will be displayed if you have the enemy flag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offsene")
	FText OffenseSecondaryMessage;

	// The text that will be displayed if you have the enemy flag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense")
	FText DefensePrimaryMessage;

	// The text that will be displayed if you have the enemy flag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defsene")
	FText DefenseSecondaryMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Down")
	FText FlagDownPrimary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Down")
	FText FlagDownOffenseSecondaryMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Down")
	FText FlagDownDefenseSecondaryMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Home")
	FText FlagHomePrimary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Home")
	FText FlagHomeOffenseSecondaryMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Home")
	FText FlagHomeDefenseSecondaryMessage;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text PrimaryMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text SecondaryMessage;

	virtual void DrawStatusMessage(float DeltaTime);


protected:
	virtual void DrawIndicators(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, float DeltaTime) override;
	virtual FText GetFlagReturnTime(AUTCTFFlag* Flag);
	virtual bool ShouldDrawFlag(AUTCTFFlag* Flag, bool bIsEnemyFlag);
	void DrawSpawnIndicator(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, AUTSCTFFlagBase* SFlagBase);
};