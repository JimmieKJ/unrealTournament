// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "UTProfileSettings.h"
#include "SUTReportUserDialog.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "UTPlayerInput.h"
#include "Scalability.h"
#include "UTWorldSettings.h"
#include "UTGameEngine.h"
#include "../SUTUtils.h"
#include "UTAnalytics.h"

#if !UE_SERVER


void SUTReportUserDialog::Construct(const FArguments& InArgs)
{
	Troll = InArgs._Troll;
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(InArgs._ButtonMask)
							.IsScrollable(false)
							.bShadow(true) 
							.OnDialogResult(InArgs._OnDialogResult)
						);


	if (DialogContent.IsValid() && Troll.IsValid())
	{
		DialogContent->AddSlot()
		.HAlign(HAlign_Center)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight().Padding(0.0f,20.0f,0.0f,100.0f)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUTReportUserDialog", "Instructions", "Please select the bad behavior you wish to report."))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.AutoWrapText(true)
			]

			+SVerticalBox::Slot().Padding(20.0f, 0.0f,20.0f,0.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().Padding(0.0,0.0f,20.0f,0.0f).FillWidth(0.5f).HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUTReportUserDialog", "AccountName", "Player Name:"))
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween")
				]
				+SHorizontalBox::Slot().Padding(20.0,0.0f,0.0f,0.0f).FillWidth(0.5f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Troll->PlayerName))
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween.Bold")
				]
			]

			+SVerticalBox::Slot().Padding(0.0f,0.0f,0.0f,80.0f).AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().Padding(0.0,0.0f,20.0f,0.0f).FillWidth(0.5f).HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUTReportUserDialog", "AccountGuid", "Unique Id:"))
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween")
				]
				+SHorizontalBox::Slot().Padding(20.0,0.0f,0.0f,0.0f).FillWidth(0.5f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Troll->UniqueId->ToString()))
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween.Bold")
				]
			]
			+SVerticalBox::Slot().Padding(0.0f,20.0f,0.0f,20.0f).AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUTReportUserDialog", "TypeofAbuse", "Type Of Abuse"))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
				.AutoWrapText(true)
			]
			+SVerticalBox::Slot().HAlign(HAlign_Center).AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().AutoWidth().Padding(0.0f,0.0f,0.0f,20.0f)
				[
					AddGridButton(NSLOCTEXT("SUTReportDialog","ReportCheat","Cheating"), 0)
				]
				+SHorizontalBox::Slot().AutoWidth().Padding(5.0f,0.0f,5.0f,20.0f)
				[
					AddGridButton(NSLOCTEXT("SUTReportDialog","ReportTrolling","Trolling"), 1)
				]
				+SHorizontalBox::Slot().AutoWidth().Padding(5.0f,0.0f,5.0f,20.0f)
				[
					AddGridButton(NSLOCTEXT("SUTReportDialog","ReportChat","Chat Abuse"), 2)
				]
				+SHorizontalBox::Slot().AutoWidth().Padding(5.0f,0.0f,0.0f,20.0f)
				[
					AddGridButton(NSLOCTEXT("SUTReportDialog","ReportGeneral","Poor Behavior"), 3)
				]
			]
		];
	}

}

TSharedRef<SWidget> SUTReportUserDialog::AddGridButton(FText Caption, int32 AbuseType)
{
	return SNew(SUTButton)
		.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton")
		.OnClicked(this, &SUTReportUserDialog::OnReportClicked, AbuseType)
		.ContentPadding(FMargin(25.0, 0.0, 25.0, 0.0))
		.Text(Caption)
		.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium");
}

//Special markup for Analytics event so they show up properly in grafana. Should be eventually moved to UTAnalytics.
/*
* @EventName AbuseReport
*
* @Trigger Players can manually send in abuse reports
*
* @Type Sent by the client
*
* @EventParam TrollName/TrollID is identifying information regarding the troll
* @EventParam ReportName/ReportID is identifying information regarding the Player Reporting the event
*
* @Comments
*/


FReply SUTReportUserDialog::OnReportClicked(int32 ReportType)
{
	if (FUTAnalytics::IsAvailable())
	{
		AUTPlayerState* PlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
		AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
		if (PlayerState && GameState)
		{
			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("TrollName"), Troll->PlayerName));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("TrollID"), Troll->UniqueId.ToString()));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("ReportName"), PlayerState->PlayerName));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("ReportID"), PlayerState->UniqueId.ToString()));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("DemoIKD"), GameState->ReplayID));

			switch (ReportType)
			{
				case 0 : ParamArray.Add(FAnalyticsEventAttribute(TEXT("Abuse"), TEXT("Cheating"))); break;
				case 1 : ParamArray.Add(FAnalyticsEventAttribute(TEXT("Abuse"), TEXT("Trolling"))); break;
				case 2 : ParamArray.Add(FAnalyticsEventAttribute(TEXT("Abuse"), TEXT("Chat Abuse"))); break;
				case 3 : ParamArray.Add(FAnalyticsEventAttribute(TEXT("Abuse"), TEXT("Poor Behavior"))); break;
			}

			FUTAnalytics::GetProvider().RecordEvent( TEXT("AbuseReport"), ParamArray );
		}
	}

	Troll->bReported = true;
	PlayerOwner->CloseAbuseDialog();

	return FReply::Handled();
}

FReply SUTReportUserDialog::OnButtonClick(uint16 ButtonID)
{
	PlayerOwner->CloseAbuseDialog();
	return FReply::Handled();
}

#endif