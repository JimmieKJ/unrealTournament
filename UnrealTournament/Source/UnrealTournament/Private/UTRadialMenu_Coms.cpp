// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTRadialMenu_Coms.h"

UUTRadialMenu_Coms::UUTRadialMenu_Coms(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	ForcedCancelDist=34.0f;
	AutoLayout(5);
	static ConstructorHelpers::FObjectFinder<UTexture2D> MenuAtlas(TEXT("Texture2D'/Game/RestrictedAssets/UI/HUDAtlas04.HUDAtlas04'"));

	YesNoTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(0.0f, 103.0f, 162.0f, 86.0f));
	
	HighlightedYesTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(162.0f, 103.0f, 162.0f, 103.0f));
	HighlightedNoTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(324.0f, 103.0f, 162.0f, 86.0f));

	IconTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(0.0f, 0.0f, 0.0f, 0.0f));

	IconUVs.Add(FTextureUVs(0.0f, 448.0f, 64.0f, 64.0f));    // Intent
	IconUVs.Add(FTextureUVs(64.0f, 448.0f, 64.0f, 64.0f));   // Defend
	IconUVs.Add(FTextureUVs(128.0f, 448.0f, 64.0f, 64.0f));  // Distress
	IconUVs.Add(FTextureUVs(192.0f, 448.0f, 64.0f, 64.0f));  // Emote
	IconUVs.Add(FTextureUVs(256.0f, 448.0f, 64.0f, 64.0f));  // Attack

	static ConstructorHelpers::FObjectFinder<UFont> YesNoFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Small.fntScoreboard_Small'"));
	YesNoTextTemplate.Font = YesNoFont.Object;
	YesNoTextTemplate.RenderColor = FLinearColor::White;
	YesNoTextTemplate.bDrawOutline = true;
	YesNoTextTemplate.OutlineColor = FLinearColor::Black;
	YesNoTextTemplate.HorzPosition = ETextHorzPos::Center;
	YesNoTextTemplate.VertPosition = ETextVertPos::Center;

	CancelCircleOpacity = 1.0f;
}

void UUTRadialMenu_Coms::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);

	// Create zones from the Yes and No segments.
	YesZone = FRadialSegment(225, 315);
	NoZone = FRadialSegment(45,135);
}

void UUTRadialMenu_Coms::BecomeInteractive()
{
	Super::BecomeInteractive();
	UWorld* CurrentWorld = UTHUDOwner->GetWorld();
	if (CurrentWorld && UTHUDOwner->UTPlayerOwner)
	{
		UTGameState = CurrentWorld->GetGameState<AUTGameState>();
		if (UTGameState != nullptr && UTGameState->GameModeClass != nullptr)
		{
			AUTGameMode* DefaultGameMode = UTGameState->GameModeClass->GetDefaultObject<AUTGameMode>();
			if (DefaultGameMode != nullptr)
			{
				ContextActor = DefaultGameMode->InitializeComMenu(CommandList, CurrentWorld, UTGameState, UTHUDOwner->UTPlayerOwner);
			}
		}
	}
}

void UUTRadialMenu_Coms::DrawMenu(FVector2D ScreenCenter, float RenderDelta)
{
	if (bIsInteractive)
	{
		float DesiredCancelCircleOpacity = (ShouldCancel() || IsNoSelected() || IsYesSelected()) ? 1.0f : 0.0f;
		if (CancelCircleOpacity < DesiredCancelCircleOpacity)
		{
			CancelCircleOpacity = FMath::Clamp<float>(CancelCircleOpacity + (RenderDelta * 5.0f), 0.0f, 1.0f);
		}
		else if (CancelCircleOpacity > DesiredCancelCircleOpacity)
		{
			CancelCircleOpacity = FMath::Clamp<float>(CancelCircleOpacity - (RenderDelta * 5.0f), 0.0f, 1.0f);
		}

		FVector2D CenterPoint = FVector2D(0.0f, -200.0f);

		for (int32 i=0; i < Segments.Num(); i++)
		{
			bool bCurrent = CurrentSegment == i && !ShouldCancel() && !IsNoSelected() && !IsYesSelected();

			float Angle = GetMidSegmentAngle(i);
			FVector2D ScreenPosition = Rotate(CenterPoint, Angle);
			SegmentTemplate.RenderScale = CalcSegmentScale(i);
			RenderObj_TextureAtWithRotation(SegmentTemplate, ScreenPosition, Angle);	
			if (bCurrent)
			{
				HighlightedSegmentTemplate.RenderScale = CalcSegmentScale(i);
				RenderObj_TextureAtWithRotation(HighlightedSegmentTemplate, ScreenPosition, Angle);	
			}

			// Draw the icon..

			IconTemplate.UVs = IconUVs[i];
			IconTemplate.RenderColor = bCurrent ? FLinearColor::Yellow : FLinearColor::White;
			FVector2D IconPosition = Rotate(FVector2D(0.0f,-210.0f), Angle);

			IconTemplate.RenderScale = CalcSegmentScale(i);
			RenderObj_Texture(IconTemplate, IconPosition);	
		}

		// Draw Yes/No

		YesNoTextTemplate.RenderOpacity = CancelCircleOpacity;
		YesNoTemplate.RenderOpacity = CancelCircleOpacity;
		HighlightedYesTemplate.RenderOpacity = CancelCircleOpacity;
		HighlightedNoTemplate.RenderOpacity = CancelCircleOpacity;

		FLinearColor YesNoTextColor = FLinearColor::White;
		FVector2D YesNoOffset = Rotate(FVector2D(0.0f, -80.0f), 90.0f);
		RenderObj_TextureAtWithRotation(YesNoTemplate, YesNoOffset, 90.0f);	
		if (IsNoSelected())
		{
			RenderObj_TextureAtWithRotation(HighlightedNoTemplate, YesNoOffset, 90.0f);	
			YesNoTextColor = FLinearColor::Yellow;
		}
		YesNoTextTemplate.RenderColor = YesNoTextColor;
		YesNoTextTemplate.Text = NSLOCTEXT("Generic","No","NO");
		RenderObj_TextAt(YesNoTextTemplate, YesNoOffset.X, YesNoOffset.Y );
		
		YesNoOffset = Rotate(FVector2D(0.0f, -80.0f), 270.0f);
		RenderObj_TextureAtWithRotation(YesNoTemplate, YesNoOffset, 270.0f);	
		
		if (IsYesSelected())
		{
			RenderObj_TextureAtWithRotation(HighlightedYesTemplate, YesNoOffset, 270.0f);	
			YesNoTextColor = FLinearColor::Yellow;
		}
		else
		{
			YesNoTextColor = FLinearColor::White;
		}
		YesNoTextTemplate.Text = NSLOCTEXT("Generic","Yes","YES");
		YesNoTextTemplate.RenderColor = YesNoTextColor;
		RenderObj_TextAt(YesNoTextTemplate, YesNoOffset.X, YesNoOffset.Y );

		// Draw the angle indicator

		RenderObj_TextureAtWithRotation(IndicatorTemplate, Rotate(FVector2D(0.0f, -140.0f), CurrentAngle), CurrentAngle);	

		if (!ShouldCancel())
		{
			CaptionTemplate.RenderOpacity = 1.0f - CancelCircleOpacity;

			// Display the text of the current zone
			FText TextToDisplay = FText::GetEmpty();

			if (IsYesSelected())
			{
				TextToDisplay = NSLOCTEXT("Generic","Yes","YES");
			}
			else if (IsNoSelected())
			{
				TextToDisplay = NSLOCTEXT("Generic","No","NO");;
			}
			else 
			{
				switch (CurrentSegment)
				{
					case 0 : TextToDisplay = CommandList.Intent.MenuText; break;
					case 4 : TextToDisplay = CommandList.Attack.MenuText; break;
					case 1 : TextToDisplay = CommandList.Defend.MenuText; break;
					case 2 : TextToDisplay = CommandList.Distress.MenuText; break;
					case 3 : TextToDisplay = FText::FromString(TEXT("Taunts")); break;
				}
			}
			CaptionTemplate.Text = TextToDisplay;
			RenderObj_Text(CaptionTemplate);
		}
	}
}

bool UUTRadialMenu_Coms::IsYesSelected()
{
	return (CurrentDistance > ForcedCancelDist && CurrentDistance <= CancelDistance && YesZone.Contains(CurrentAngle));
}
bool UUTRadialMenu_Coms::IsNoSelected()
{
	return (CurrentDistance > ForcedCancelDist && CurrentDistance <= CancelDistance && NoZone.Contains(CurrentAngle));
}


bool UUTRadialMenu_Coms::ShouldCancel()
{
	return CurrentDistance <= ForcedCancelDist || (CurrentDistance <= CancelDistance && !IsYesSelected() && !IsNoSelected());
}
	
void UUTRadialMenu_Coms::UpdateSegment()
{
	if (IsNoSelected() || IsYesSelected())
	{
		if (CurrentSegment >=0)
		{
			ChangeSegment(-1);
		}
		return;
	}

	Super::UpdateSegment();
}


void UUTRadialMenu_Coms::Execute()
{

	FString ConsoleCommand = TEXT("");

	UWorld* CurrentWorld = UTHUDOwner->GetWorld();
	if (CurrentWorld && UTHUDOwner->UTPlayerOwner)
	{
		UTGameState = CurrentWorld->GetGameState<AUTGameState>();
		if (UTGameState != nullptr && UTGameState->GameModeClass != nullptr)
		{
			AUTGameMode* DefaultGameMode = UTGameState->GameModeClass->GetDefaultObject<AUTGameMode>();
			if (DefaultGameMode != nullptr)
			{
				if (IsYesSelected())
				{
					DefaultGameMode->ExecuteComMenu(CommandTags::Yes, CurrentWorld, UTGameState, UTHUDOwner->UTPlayerOwner, ContextActor);
				}
				else if (IsNoSelected()) 
				{
					DefaultGameMode->ExecuteComMenu(CommandTags::No, CurrentWorld, UTGameState, UTHUDOwner->UTPlayerOwner, ContextActor);
				}
				else
				{
					switch (CurrentSegment)
					{
						case 0 : DefaultGameMode->ExecuteComMenu(CommandTags::Intent, CurrentWorld, UTGameState, UTHUDOwner->UTPlayerOwner, ContextActor); break;
						case 4 : DefaultGameMode->ExecuteComMenu(CommandTags::Attack, CurrentWorld, UTGameState, UTHUDOwner->UTPlayerOwner, ContextActor); break;
						case 1 : DefaultGameMode->ExecuteComMenu(CommandTags::Defend, CurrentWorld, UTGameState, UTHUDOwner->UTPlayerOwner, ContextActor); break;
						case 2 : DefaultGameMode->ExecuteComMenu(CommandTags::Distress, CurrentWorld, UTGameState, UTHUDOwner->UTPlayerOwner, ContextActor); break;
					}
				}
			}
		}
	}
}

void UUTRadialMenu_Coms::ChangeSegment(int32 NewSegmentIndex)
{
	BounceTimer = COMS_ANIM_TIME;
	Super::ChangeSegment(NewSegmentIndex);
}