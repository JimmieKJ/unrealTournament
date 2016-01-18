// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget_Intermission.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_Intermission : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Draw_Implementation(float DeltaTime) override;
	virtual bool ShouldDraw_Implementation(bool bShowScores) override;

	UPROPERTY()
		FText AssistedByText;
	UPROPERTY()
		FText UnassistedText;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SlideOut")
		UTexture2D* TextureAtlas;
};
