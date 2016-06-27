// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 **/

#include "UTHUDWidget_Boost.generated.h"

const float FLOATING_NUMBER_ANIN_TIME = 0.75f;

struct FCurrentChange
{
	int32 Amount;
	float Timer;

	FCurrentChange(int32 inAmount)
		: Amount(inAmount)
		, Timer(FLOATING_NUMBER_ANIN_TIME)
	{
	}

};

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_Boost : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void InitializeWidget(AUTHUD* Hud);
	virtual void Draw_Implementation(float DeltaTime);
	virtual bool ShouldDraw_Implementation(bool bShowScores);
	
private:
	UPROPERTY()
	UMaterialInterface* HudTimerMI;

	UPROPERTY()
	UMaterialInstanceDynamic* HudTimerMID;
	float IconScale;
	float LastCurrency;

protected:

	TArray<FCurrentChange> FloatingNumbers;

};
