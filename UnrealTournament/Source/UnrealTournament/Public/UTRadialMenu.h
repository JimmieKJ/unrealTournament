// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget.h"
#include "UTRadialMenu.generated.h"

const float COMS_ANIM_TIME = 0.2f;			
const float COMS_SELECTED_SEGMENT_SCALE = 1.2f;


USTRUCT()
struct FRadialSegment
{
	GENERATED_USTRUCT_BODY()


	/**  X portion of the vector is the start angle, Y portion is the end */
	UPROPERTY()
	TArray<FVector2D> Angles;

	FRadialSegment()
	{}

	FRadialSegment(float StartAngle, float EndAngle)
	{
		Angles.Add(FVector2D(StartAngle, EndAngle));
	}

	// One of the cells will always require a split so make it easy to setup
	FRadialSegment(float StartAngle, float EndAngle, float SecondStartAngle, float SecondEndAngle)
	{
		Angles.Add(FVector2D(StartAngle, EndAngle));
		Angles.Add(FVector2D(SecondStartAngle, SecondEndAngle));
	}

	bool Contains(float Angle)
	{
		for (int32 i = 0; i < Angles.Num(); i++ )	
		{
			if (Angle >= Angles[i].X && Angle <= Angles[i].Y)
			{
				return true;
			}
		}

		return false;
	}

};

UCLASS()
class UNREALTOURNAMENT_API UUTRadialMenu : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture WeaponIconTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture InnerCircleTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture SegmentTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture HighlightedSegmentTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture IndicatorTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text CaptionTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture CursorTemplate;

	UPROPERTY()
	float CancelDistance;

	virtual FVector2D Rotate(FVector2D ScreenPosition, float Angle);

	// The main drawing stub
	virtual void Draw_Implementation(float DeltaTime);
	virtual bool IsInteractive() { return bIsInteractive; };

	virtual void BecomeInteractive();
	virtual void BecomeNonInteractive();

protected:

	float BounceTimer;
	float BounceRate;

	// returns true to cancel this menu and do no action.
	virtual bool ShouldCancel();

	// This function will be called when the menu is executed.
	virtual void Execute();

	// The Main drawing code goes here.  Should draw each cell of the menu
	virtual void DrawMenu(FVector2D ScreenCenter, float RenderDelta) {};

	// Draws the cursor.  It will be called after DrawMenu and any relevant debug information
	virtual void DrawCursor(FVector2D ScreenCenter, float RenderDelta);

	bool bIsInteractive;
	FVector2D CursorPosition;

	bool bDrawDebug;

	float CurrentAngle;
	float CurrentDistance;
	int32 CurrentSegment;

	TArray<FRadialSegment> Segments;

	virtual void AutoLayout(int32 NumberOfSegments);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text DebugTextTemplate;

	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
		return UTHUDOwner && UTHUDOwner->bShowComsMenu;
	}

	float GetMidSegmentAngle(int32 SegmentIndex);

	virtual void UpdateSegment();
	virtual void ChangeSegment(int32 NewSegmentIndex);
	virtual float CalcSegmentScale(int32 SegmentIndex);

public:
	virtual bool ProcessInputAxis(FKey Key, float Delta);
	virtual bool ProcessInputKey(FKey Key, EInputEvent EventType);

};

