// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget_NetInfo.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_NetInfo : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Draw_Implementation(float DeltaTime);
	virtual bool ShouldDraw_Implementation(bool bShowScores);

	// The total Height of a given cell
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
		float CellHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
		float DataColumnX;

	virtual FLinearColor GetValueColor(float Value, float ThresholdBest, float ThresholdWorst) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
		UTexture2D* TextureAtlas;

	UPROPERTY()
		FLinearColor ValueHighlight[3];
};
