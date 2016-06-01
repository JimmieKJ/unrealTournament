// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget.h"
#include "UTRadialMenu.h"
#include "UTRadialMenu_Coms.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTRadialMenu_Coms : public UUTRadialMenu
{
	GENERATED_UCLASS_BODY()

public:
	virtual void InitializeWidget(AUTHUD* Hud);
	virtual void BecomeInteractive();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture YesNoTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture HighlightedYesTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture HighlightedNoTemplate;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture IconTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FTextureUVs> IconUVs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text YesNoTextTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float ForcedCancelDist;
protected:

	AActor* ContextActor;
	float CancelCircleOpacity;

	FComMenuCommandList CommandList;
	FRadialSegment YesZone;
	FRadialSegment NoZone;

	virtual bool IsYesSelected();
	virtual bool IsNoSelected();

	virtual void DrawMenu(FVector2D ScreenCenter, float RenderDelta);
	virtual bool ShouldCancel();
	virtual void Execute();

	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
		return UTHUDOwner && UTHUDOwner->bShowComsMenu;
	}

	virtual void ChangeSegment(int32 NewSegmentIndex);

	virtual void UpdateSegment();

};

