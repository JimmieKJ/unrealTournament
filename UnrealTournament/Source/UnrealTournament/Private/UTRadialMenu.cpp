// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTRadialMenu.h"

UUTRadialMenu::UUTRadialMenu(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	Position = FVector2D(0.f, 0.f);
	Size = FVector2D(1900.0f, 1080.0f);
	ScreenPosition = FVector2D(0.5f, 0.5f);
	Origin = FVector2D(0.f, 0.f);
	bIsInteractive = false;
	bIgnoreHUDOpacity = true;
	bDrawDebug = false;
	CancelDistance = 120.0f;

	static ConstructorHelpers::FObjectFinder<UTexture2D> HudAtlas(TEXT("Texture2D'/Game/RestrictedAssets/UI/HUDAtlas01.HUDAtlas01'"));
	CursorTemplate.Atlas = HudAtlas.Object;
	CursorTemplate.RenderOffset = FVector2D(0.5f, 0.5f);
	CursorTemplate.UVs = FTextureUVs(894,38,26,25);
	CursorTemplate.Size = FVector2D(26,25);

	static ConstructorHelpers::FObjectFinder<UTexture2D> MenuAtlas(TEXT("Texture2D'/Game/RestrictedAssets/UI/HUDAtlas04.HUDAtlas04'"));

	InnerCircleTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(0.0f, 206.0f, 236.0f, 236.0f));
	SegmentTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(0.0f, 0.0f, 229.0f, 102.0f));
	HighlightedSegmentTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(229.0f, 0.0f, 229.0f, 102.0f));
	IndicatorTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(0.0f, 189.0f, 108.0f, 17.0f));

	static ConstructorHelpers::FObjectFinder<UFont> CaptionFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Medium.fntScoreboard_Medium'"));
	CaptionTemplate.Font = CaptionFont.Object;
	CaptionTemplate.RenderColor = FLinearColor::White;
	CaptionTemplate.bDrawOutline = true;
	CaptionTemplate.OutlineColor = FLinearColor::Black;
	CaptionTemplate.HorzPosition = ETextHorzPos::Center;
	CaptionTemplate.Position.Y = -32.0f; //300;

	DebugTextTemplate.Font = CaptionFont.Object;
	DebugTextTemplate.RenderColor = FLinearColor::White;
	DebugTextTemplate.bDrawOutline = true;
	DebugTextTemplate.OutlineColor = FLinearColor::Black;
	DebugTextTemplate.HorzPosition = ETextHorzPos::Center;

	BounceTimer = 0.0f;
	BounceRate = 5.0f; // 1/20 of a second

}

void UUTRadialMenu::AutoLayout(int32 NumberOfSegments)
{
	float SegmentSize = 360.0f / float(NumberOfSegments);
	
	float Start = SegmentSize * 0.5f;

	// The first segment is tricky in that it crosses the 360/0 boundry.  

	Segments.Add(FRadialSegment(360.0f - Start, 360.0f, 0.0f, Start));
	for (int32 i = 1; i < NumberOfSegments; i++)
	{
		float End = Start + SegmentSize;
		Segments.Add(FRadialSegment(Start,End));
		Start = End;
	}
}

float UUTRadialMenu::CalcSegmentScale(int32 SegmentIndex)
{
	if (SegmentIndex == CurrentSegment) 
	{
		return BounceEaseOut(1.0f, COMS_SELECTED_SEGMENT_SCALE, 1.0f - (BounceTimer / COMS_ANIM_TIME),1.25);
	}
	return 1.0f;
}


float UUTRadialMenu::GetMidSegmentAngle(int32 SegmentIndex)
{
	if (Segments.IsValidIndex(SegmentIndex))
	{
		return (Segments[SegmentIndex].Angles.Num() > 1) ? Segments[SegmentIndex].Angles[1].X : Segments[SegmentIndex].Angles[0].X + ((Segments[SegmentIndex].Angles[0].Y - Segments[SegmentIndex].Angles[0].X) * 0.5f);
	}

	return 0.0f;
}


FVector2D UUTRadialMenu::Rotate(FVector2D InScreenPosition, float Angle)
{
	float Sin = 0.f;
	float Cos = 0.f;

	FMath::SinCos(&Sin, &Cos, FMath::DegreesToRadians(Angle));
	FVector2D NewScreenPosition;
	
	NewScreenPosition.X = InScreenPosition.X * Cos - InScreenPosition.Y * Sin;
	NewScreenPosition.Y = InScreenPosition.Y * Cos + InScreenPosition.X * Sin;
	return NewScreenPosition;
}

void UUTRadialMenu::Draw_Implementation(float RenderDelta)
{
	if (bIsInteractive)
	{
		FVector2D ScreenCenter = FVector2D(Canvas->ClipX * 0.5, Canvas->ClipY * 0.5);
		CursorPosition.X = FMath::Clamp<float>(CursorPosition.X, ScreenCenter.X * -1.0f, ScreenCenter.X);
		CursorPosition.Y = FMath::Clamp<float>(CursorPosition.Y, ScreenCenter.Y * -1.0f, ScreenCenter.Y);

		if (BounceTimer > 0.0f)
		{
			BounceTimer -= RenderDelta;
		}

		RenderObj_Texture(InnerCircleTemplate, FVector2D(0.0f,0.0f));

		DrawMenu(ScreenCenter, RenderDelta);

		RenderObj_TextureAtWithRotation(IndicatorTemplate, Rotate(FVector2D(0.0f, -140.0f), CurrentAngle), CurrentAngle);	

		if (bDrawDebug)
		{
			FVector2D OriginLine = FVector2D(0.0f, -300.0f);
			for (int32 i=0; i < Segments.Num(); i++ )
			{
				float StartAngle = Segments[i].Angles[0].X;
				float EndAngle = Segments[i].Angles.Num() > 1 ? Segments[i].Angles[1].Y : Segments[i].Angles[0].Y;
		
				FVector2D DrawScreenPosition = Rotate(OriginLine, StartAngle);
				Canvas->K2_DrawLine(ScreenCenter, ScreenCenter + DrawScreenPosition, 1.0, FLinearColor::White);
				ScreenPosition = Rotate(OriginLine, EndAngle);
				Canvas->K2_DrawLine(ScreenCenter, ScreenCenter + DrawScreenPosition, 1.0, FLinearColor::White);
			}

			DebugTextTemplate.Text = FText::Format(NSLOCTEXT("UTRadialMenu","TestFormat","Angle = {0} Dist = {1}  Segment= {2}"), FText::AsNumber(CurrentAngle), FText::AsNumber(CurrentDistance), FText::AsNumber(CurrentSegment));
			RenderObj_TextAt(DebugTextTemplate, 0, 400);
		}

		DrawCursor(ScreenCenter, RenderDelta);
	}
}

void UUTRadialMenu::DrawCursor(FVector2D ScreenCenter, float RenderDelta)
{
	// Draw the Cursor
	RenderObj_TextureAt(CursorTemplate, CursorPosition.X, CursorPosition.Y, 26, 25);
}

void UUTRadialMenu::BecomeInteractive()
{
	CursorPosition = FVector2D(0.0f, 0.0f);
	bIsInteractive = true;
}

bool UUTRadialMenu::ShouldCancel()
{
	return CurrentDistance <= CancelDistance;
}


void UUTRadialMenu::Execute()
{
}

void UUTRadialMenu::BecomeNonInteractive()
{

	if ( !ShouldCancel() )
	{
		Execute();
	}
	bIsInteractive = false;
}

bool UUTRadialMenu::ProcessInputAxis(FKey Key, float Delta)
{
	bool bUpdate = false;
	if (IsInteractive())
	{
		if (Key == EKeys::MouseX)
		{
			CursorPosition.X += Delta;
			bUpdate = true;
		}
		else if (Key == EKeys::MouseY)
		{
			CursorPosition.Y -= Delta;
			bUpdate = true;
		}

		if (bUpdate)
		{
			CurrentDistance = FMath::Abs<float>(CursorPosition.Size());
			CurrentAngle = FMath::RadiansToDegrees(FMath::Atan2(CursorPosition.X, CursorPosition.Y * -1));
			if (CurrentAngle < 0.0f) CurrentAngle = 360.0f + CurrentAngle;

			UpdateSegment();
		}
	}

	return bIsInteractive;
}

void UUTRadialMenu::UpdateSegment()
{
	if (ShouldCancel())
	{
		if (CurrentSegment >= 0)
		{
			ChangeSegment(-1);
		}
	}
	else
	{
		for (int32 i=0; i < Segments.Num(); i++)
		{
			if (Segments[i].Contains(CurrentAngle))
			{
				if (i != CurrentSegment)
				{
					ChangeSegment(i);
				}
				break;
			}
		}
	}
}


void UUTRadialMenu::ChangeSegment(int32 NewSegmentIndex)
{
	CurrentSegment = NewSegmentIndex;
}

bool UUTRadialMenu::ProcessInputKey(FKey Key, EInputEvent EventType)
{
	if (IsInteractive())
	{
	}
	return false;
}
