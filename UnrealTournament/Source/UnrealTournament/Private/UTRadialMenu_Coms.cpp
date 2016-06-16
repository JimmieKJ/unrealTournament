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

	static ConstructorHelpers::FObjectFinder<UFont> ToolTipFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Medium.fntScoreboard_Medium'"));
	ToolTipTemplate.Font = ToolTipFont.Object;
	ToolTipTemplate.RenderColor = FLinearColor::Yellow;
	ToolTipTemplate.bDrawOutline = true;
	ToolTipTemplate.OutlineColor = FLinearColor::Black;
	ToolTipTemplate.HorzPosition = ETextHorzPos::Center;
	ToolTipTemplate.Position.Y = 300;

	CommandList.Intent		= FComMenuCommand(NSLOCTEXT("ComCommands","Intent","Intent"), CommandTags::Intent);
	CommandList.Attack		= FComMenuCommand(NSLOCTEXT("ComCommands","Attack","Attack"), CommandTags::Attack);
	CommandList.Defend		= FComMenuCommand(NSLOCTEXT("ComCommands","Defend","Defend"), CommandTags::Defend);
	CommandList.Distress	= FComMenuCommand(NSLOCTEXT("ComCommands","Distress","Distress"), CommandTags::Distress);

	CancelCircleOpacity = 1.0f;
}

void UUTRadialMenu_Coms::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);

	// Create zones from the Yes and No segments.
	YesZone = FRadialSegment(225, 315);
	NoZone = FRadialSegment(45,135);
}

void UUTRadialMenu_Coms::GetComData(FName CommandTag, AActor* ContextActor, AUTPlayerController* Instigator)
{
	if (GetWorld() && GetWorld()->GetGameState())
	{
		AUTGameMode* DefaultGameMode = Cast<AUTGameMode>(GetWorld()->GetGameState()->GameModeClass.GetDefaultObject());
		if (DefaultGameMode)
		{
			AUTPlayerState* UTPlayerState = UTHUDOwner->UTPlayerOwner->UTPlayerState;
			if (UTPlayerState)
			{
				int32 Switch = DefaultGameMode->GetComSwitch(CommandTag, ContextActor, Instigator, GetWorld());
				if (Switch != INDEX_NONE)
				{
					UUTCharacterVoice* DefaultCharacterVoice = Cast<UUTCharacterVoice>(UTPlayerState->GetCharacterVoiceClass().GetDefaultObject());
					if (DefaultCharacterVoice)
					{					
						ComData.Add(CommandTag, FComData(DefaultCharacterVoice->GetText(Switch, true, UTPlayerState, nullptr, UTPlayerState->LastKnownLocation),Switch));
						return;
					}
				}
			}
		}
	}
	ComData.Add(CommandTag, FComData(FText::GetEmpty(),INDEX_NONE));
}


void UUTRadialMenu_Coms::BecomeInteractive()
{
	Super::BecomeInteractive();

	ComData.Empty();

	ContextActor = nullptr;
	if (UTHUDOwner && UTHUDOwner->UTPlayerOwner)
	{
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetUTCharacter());
		if (UTCharacter)
		{
			ContextActor = UTCharacter->GetCurrentAimContext();
		}
	}
	GetComData(CommandTags::Intent, ContextActor, UTHUDOwner->UTPlayerOwner);
	GetComData(CommandTags::Attack, ContextActor, UTHUDOwner->UTPlayerOwner);
	GetComData(CommandTags::Defend, ContextActor, UTHUDOwner->UTPlayerOwner);
	GetComData(CommandTags::Distress, ContextActor, UTHUDOwner->UTPlayerOwner);
	GetComData(CommandTags::Yes, ContextActor, UTHUDOwner->UTPlayerOwner);
	GetComData(CommandTags::No, ContextActor, UTHUDOwner->UTPlayerOwner);
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
			FText ToolTipText = FText::GetEmpty();
			if (IsYesSelected())
			{
				TextToDisplay = NSLOCTEXT("Generic","Yes","YES");
				ToolTipText = ComData[CommandTags::Yes].ToolTip;

			}
			else if (IsNoSelected())
			{
				TextToDisplay = NSLOCTEXT("Generic","No","NO");;
				ToolTipText = ComData[CommandTags::No].ToolTip;
			}
			else 
			{
				switch (CurrentSegment)
				{
					case 0 : 
						TextToDisplay = CommandList.Intent.MenuText; 
						ToolTipText = ComData[CommandTags::Intent].ToolTip;
						break;

					case 4 : 
						TextToDisplay = CommandList.Attack.MenuText; 
						ToolTipText = ComData[CommandTags::Attack].ToolTip;
						break;
					case 1 : 
						TextToDisplay = CommandList.Defend.MenuText; 
						ToolTipText = ComData[CommandTags::Defend].ToolTip;
						break;
					case 2 : 
						TextToDisplay = CommandList.Distress.MenuText; 
						ToolTipText = ComData[CommandTags::Distress].ToolTip;
						break;
					case 3 : 
						TextToDisplay = FText::FromString(TEXT("Taunts")); 
						break;
				}

			}
			CaptionTemplate.Text = TextToDisplay;
			RenderObj_Text(CaptionTemplate);
		
			ToolTipTemplate.Text = ToolTipText;
			if (!ToolTipText.IsEmpty())
			{
				RenderObj_Text(ToolTipTemplate);
			}
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

	if (UTHUDOwner == nullptr || UTHUDOwner->UTPlayerOwner == nullptr) return;

	int32 ComSwitch = INDEX_NONE;

	if (IsYesSelected()) ComSwitch = ComData[CommandTags::Yes].Switch;
	else if (IsNoSelected()) ComSwitch = ComData[CommandTags::No].Switch;
	else
	{
		switch (CurrentSegment)
		{
			case 0 : ComSwitch = ComData[CommandTags::Intent].Switch; break;
			case 4 : ComSwitch = ComData[CommandTags::Attack].Switch; break;
			case 1 : ComSwitch = ComData[CommandTags::Defend].Switch; break;
			case 2 : ComSwitch = ComData[CommandTags::Distress].Switch; break;
		}
	}

	if (ComSwitch != INDEX_NONE)
	{
		AUTCharacter* ContextCharacter = ContextActor != nullptr ? Cast<AUTCharacter>(ContextActor) : nullptr;
		AUTPlayerState* ContextPlayerState = ContextCharacter != nullptr ? Cast<AUTPlayerState>(ContextCharacter->PlayerState) : nullptr;
		UTHUDOwner->UTPlayerOwner->ServerSendComsMessage(ContextPlayerState, ComSwitch);
	}
}

void UUTRadialMenu_Coms::ChangeSegment(int32 NewSegmentIndex)
{
	BounceTimer = COMS_ANIM_TIME;
	Super::ChangeSegment(NewSegmentIndex);
}