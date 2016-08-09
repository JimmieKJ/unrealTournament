// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget_NetInfo.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_NetInfo : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Draw_Implementation(float DeltaTime);
	virtual bool ShouldDraw_Implementation(bool bShowScores);

	virtual void AddPing(float NewPing);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
		float DataColumnX;

	virtual FLinearColor GetValueColor(float Value, float ThresholdBest, float ThresholdWorst) const;

protected:
	UPROPERTY()
		FLinearColor ValueHighlight[3];

	UPROPERTY()
		int32 CurrentBucket;

	UPROPERTY()
		int32 BucketIndex;

	UPROPERTY()
		float PingHistory[300];

	UPROPERTY()
		float MaxDeviation;

	UPROPERTY()
		int32 NumPingsRcvd;

	UPROPERTY()
		int32 PacketsIn;

	UPROPERTY()
		int32 PacketsOut;

	UPROPERTY()
		int32 LastPacketsIn;

	UPROPERTY()
		int32 LastPacketsOut;

	virtual float CalcAvgPing();
	virtual float CalcPingStandardDeviation(float AvgPing);

};
