
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "../SUWScaleBox.h"
#include "SlateExtras.h"
#include "Slate/SlateGameResources.h"
#include "SUTFragCenterPanel.h"
#include "Engine/UserInterfaceSettings.h"

#if !UE_SERVER

void SUTFragCenterPanel::ConstructPanel(FVector2D ViewportSize)
{
	SUTWebBrowserPanel::ConstructPanel(ViewportSize);

	if (Overlay.IsValid())
	{
		Overlay->AddSlot()
			[
				SNew(SCanvas)
				+SCanvas::Slot()
				.Size(FVector2D(300,60))
				.Position(FVector2D(1700,725))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBox)
							.WidthOverride(150)
							.HeightOverride(32)
							[
								SAssignNew(AutoPlayToggleButton, SUTButton)
								.OnClicked(this, &SUTFragCenterPanel::ChangeAutoPlay)
								.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
								//.ToolTip(SUTUtils::CreateTooltip(AvailableItems[Idx]->DefaultObject->DisplayName))
								//.UTOnMouseOver(this, &SUTBuyMenu::AvailableUpdateItemInfo)
								//.WidgetTag(Idx)
								[
									SNew(STextBlock)
									.Text( TAttribute<FText>::Create( TAttribute<FText>::FGetter::CreateSP( this, &SUTFragCenterPanel::GetAutoPlayText)))
									.TextStyle(SUWindowsStyle::Get(), "UT.Hub.RulesText_Small")
									.ColorAndOpacity(FLinearColor::Black)
								]
							]
						]
					]
				]
			];
	}
}

FText SUTFragCenterPanel::GetAutoPlayText()
{
	return GetPlayerOwner()->bFragCenterAutoPlay ? NSLOCTEXT("SUTFragCenterPanel","AutoPlayOn","[AutoPlay: ON]") : NSLOCTEXT("SUTFragCenterPanel","AutoPlayOff","[AutoPlay: OFF]");
}

FReply SUTFragCenterPanel::ChangeAutoPlay()
{
	GetPlayerOwner()->bFragCenterAutoPlay = !GetPlayerOwner()->bFragCenterAutoPlay;
	GetPlayerOwner()->SaveConfig();
	UpdateAutoPlay();

	return FReply::Handled();	
}

void SUTFragCenterPanel::UpdateAutoPlay()
{
	FString JavaCommand = FString::Printf( TEXT("setAutoStart(%s);"), (GetPlayerOwner()->bFragCenterAutoPlay ? TEXT("true") : TEXT("false")) );
	ExecuteJavascript(JavaCommand);
}

bool SUTFragCenterPanel::OnBrowse(FString TargetURL, bool bRedirect)
{
	if ( TargetURL.Contains(TEXT("necris.net"),ESearchCase::IgnoreCase) || TargetURL.Contains(TEXT("syndication.twitter.com"),ESearchCase::IgnoreCase) )
	{
		return false;
	}

	UE_LOG(UT,Log, TEXT("Rejecting %s"), *TargetURL);
	return true;

}


#endif