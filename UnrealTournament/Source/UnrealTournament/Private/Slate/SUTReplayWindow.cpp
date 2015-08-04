// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "../Public/UTGameViewportClient.h"
#include "../Public/UTHUDWidget_SpectatorSlideOut.h"
#include "SUTReplayWindow.h"
#include "SUWindowsStyle.h"
#include "Widgets/SUTButton.h"
#include "SNumericEntryBox.h"
#include "Widgets/SUTProgressSlider.h"
#include "UTPlayerState.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/DemoNetDriver.h"
#include "Runtime/NetworkReplayStreaming/NetworkReplayStreaming/Public/NetworkReplayStreaming.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "UTVideoRecordingFeature.h"
#include "SUWScreenshotConfigDialog.h"
#include "SceneViewport.h"

#if !UE_SERVER

void SUTReplayWindow::Construct(const FArguments& InArgs)
{
	SpeedMin = 0.1f;
	SpeedMax = 2.0f;

	RecordTimeStart = -1;
	RecordTimeStop = -1;

	PlayerOwner = InArgs._PlayerOwner;
	DemoNetDriver = InArgs._DemoNetDriver;
	checkSlow(PlayerOwner != nullptr);
	checkSlow(DemoNetDriver != nullptr);

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	float DPIScale = GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass())->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));
	ViewportSize = ViewportSize / DPIScale;

	FVector2D TimeSize = FVector2D(0.5f, 0.09f) * ViewportSize;
	FVector2D TimePos(ViewportSize.X * 0.5f - TimeSize.X * 0.5f, ViewportSize.Y * 0.8);

	HideTimeBarTime = 5.0f;
	bDrawTooltip = false;

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
		.Visibility(this, &SUTReplayWindow::GetVis)
		+ SOverlay::Slot()
		[
			//TimeSlider tooltip
			SNew(SCanvas)
			+ SCanvas::Slot()
			.Position(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateSP(this, &SUTReplayWindow::GetTimeTooltipPosition)))
			.Size(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateSP(this, &SUTReplayWindow::GetTimeTooltipSize)))
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBorder)
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					.BorderImage(SUWindowsStyle::Get().GetBrush("UT.Replay.Tooltip.BG"))
					.Content()
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Text(this, &SUTReplayWindow::GetTooltipText)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					]
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UT.Replay.Tooltip.Arrow"))
				]
			]
		]
		//Time Controls
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1.0f)

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			[
				SAssignNew(TimeBar, SBorder)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.ColorAndOpacity(this, &SUTReplayWindow::GetTimeBarColor)
				.BorderBackgroundColor(this, &SUTReplayWindow::GetTimeBarBorderColor)
				.BorderImage(SUWindowsStyle::Get().GetBrush("UT.TopMenu.Shadow"))
				.Content()
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					.Padding(10.0f, 0.0f, 10.0f, 10.0f)
					[
						//The Main time slider
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Fill)
						.AutoHeight()
						.Padding(0.0f,0.0f,0.0f,10.0f)
						[
							SAssignNew(TimeSlider, SUTProgressSlider)
							.Value(this, &SUTReplayWindow::GetTimeSlider)
							.TotalValue(this, &SUTReplayWindow::GetTimeSliderLength)
							.OnValueChanged(this, &SUTReplayWindow::OnSetTimeSlider)
							.SliderBarColor(FColor(33, 93, 220))
							.SliderBarBGColor(FLinearColor(0.05f,0.05f,0.05f))
						]
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Fill)
						.AutoHeight()
						.Padding(50.0f,0.0f)
						[
							//The current / total time
							SNew(SUniformGridPanel)
							+ SUniformGridPanel::Slot(0, 0)
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.HAlign(HAlign_Fill)
								.AutoHeight()
								.Padding(0.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(this, &SUTReplayWindow::GetTimeText)
									.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
								]
								+ SVerticalBox::Slot()
								.HAlign(HAlign_Fill)
								.AutoHeight()
								.Padding(0.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Left)
									.AutoWidth()
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										.HAlign(HAlign_Fill)
										.AutoHeight()
										[
											SNew(SBox)
											.HeightOverride(32)
											.WidthOverride(32)
											[
												SAssignNew(RecordButton, SButton)
												.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
												.OnClicked(this, &SUTReplayWindow::OnMarkRecordStartClicked)
												.Content()
												[
													SNew(SImage)
													.Image(SUWindowsStyle::Get().GetBrush("UT.Replay.Button.MarkStart"))
												]
											]
										]
									]
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Left)
									.AutoWidth()
									[											
										SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										.HAlign(HAlign_Fill)
										.AutoHeight()
										[
											SNew(SBox)
											.HeightOverride(32)
											.WidthOverride(32)
											[
												SAssignNew(MarkStartButton, SButton)
												.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
												.OnClicked(this, &SUTReplayWindow::OnMarkRecordStopClicked)
												.Content()
												[
													SNew(SImage)
													.Image(SUWindowsStyle::Get().GetBrush("UT.Replay.Button.MarkEnd"))
												]
											]
										]
									]
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Left)
									.AutoWidth()
									[											
										SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										.HAlign(HAlign_Fill)
										.AutoHeight()
										[
											SNew(SBox)
											.HeightOverride(32)
											.WidthOverride(32)
											[
												SAssignNew(MarkEndButton, SButton)
												.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
												.OnClicked(this, &SUTReplayWindow::OnRecordButtonClicked)
												.Content()
												[
													SNew(SImage)
													.Image(SUWindowsStyle::Get().GetBrush("UT.Replay.Button.Record"))
												]
											]
										]
									]
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Left)
									.AutoWidth()
									[									
										SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										.HAlign(HAlign_Fill)
										.AutoHeight()
										[
											SNew(SBox)
											.HeightOverride(32)
											.WidthOverride(32)
											[
												SAssignNew(MarkEndButton, SButton)
												.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
												.OnClicked(this, &SUTReplayWindow::OnScreenshotButtonClicked)
												.Content()
												[
													SNew(SImage)
													.Image(SUWindowsStyle::Get().GetBrush("UT.Replay.Button.Screenshot"))
												]
											]
										]
									]
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Left)
									.AutoWidth()
									[									
										SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										.HAlign(HAlign_Fill)
										.AutoHeight()
										[
											SNew(SBox)
											.HeightOverride(32)
											.WidthOverride(32)
											[
												SAssignNew(MarkEndButton, SButton)
												.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
												.OnClicked(this, &SUTReplayWindow::OnScreenshotConfigButtonClicked)
												.Content()
												[
													SNew(SImage)
													.Image(SUWindowsStyle::Get().GetBrush("UT.Replay.Button.ScreenshotConfig"))
												]
											]
										]
									]
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Left)
									.AutoWidth()
									.Padding(10.0f, 0.0f, 10.0f, 0.0f)
									[
										SAssignNew(BookmarksComboBox, SComboBox< TSharedPtr<FString> >)
										.InitiallySelectedItem(0)
										.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
										.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
										.OptionsSource(&BookmarkNameList)
										.OnGenerateWidget(this, &SUTReplayWindow::GenerateStringListWidget)
										.OnSelectionChanged(this, &SUTReplayWindow::OnBookmarkSetSelected)
										.Content()
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot()
											.Padding(10.0f, 0.0f, 10.0f, 0.0f)
											[
												SAssignNew(SelectedBookmark, STextBlock)
												.Text(FText::FromString(TEXT("Bookmarks")))
												.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
											]
										]
									]
								]
							]
							//Play / Pause button
							+ SUniformGridPanel::Slot(1, 0)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(SButton)
								.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
								.OnClicked(this, &SUTReplayWindow::OnPlayPauseButtonClicked)
								.ContentPadding(1.0f)
								.Content()
								[
									SNew(SImage)
									.Image(this, &SUTReplayWindow::GetPlayButtonBrush)
								]
							]
			
							//Speed Control
							+ SUniformGridPanel::Slot(2, 0)
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							[
								SNew(SBox)
								.WidthOverride(170)
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Center)
								.Content()
								[
									SNew(SNumericEntryBox<float>)
									.AllowSpin(true)
									.MinValue(SpeedMin)
									.MaxValue(SpeedMax)
									.MinSliderValue(SpeedMin)
									.MaxSliderValue(SpeedMax)
									.Value(this, &SUTReplayWindow::GetSpeedSlider)
									.OnValueChanged(this, &SUTReplayWindow::OnSetSpeedSlider)
									.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.NumEditbox.White")
								]
							]
						]
					]
				]
			]
		]
	];

	bool bVideoRecorderPresent = false;
	static const FName VideoRecordingFeatureName("VideoRecording");
	if (IModularFeatures::Get().IsModularFeatureAvailable(VideoRecordingFeatureName))
	{
		UTVideoRecordingFeature* VideoRecorder = &IModularFeatures::Get().GetModularFeature<UTVideoRecordingFeature>(VideoRecordingFeatureName);
		if (VideoRecorder)
		{
			bVideoRecorderPresent = true;
		}
	}

	if (!bVideoRecorderPresent)
	{
		RecordButton->SetVisibility(EVisibility::Hidden);
		MarkStartButton->SetVisibility(EVisibility::Hidden);
		MarkEndButton->SetVisibility(EVisibility::Hidden);
	}

	if (DemoNetDriver.IsValid())
	{
		FEnumerateEventsCompleteDelegate EnumKills = FEnumerateEventsCompleteDelegate::CreateSP(this, &SUTReplayWindow::KillsEnumerated);
		FEnumerateEventsCompleteDelegate EnumFlagCaps = FEnumerateEventsCompleteDelegate::CreateSP(this, &SUTReplayWindow::FlagCapsEnumerated);
		FEnumerateEventsCompleteDelegate EnumFlagReturns = FEnumerateEventsCompleteDelegate::CreateSP(this, &SUTReplayWindow::FlagReturnsEnumerated);
		FEnumerateEventsCompleteDelegate EnumFlagDeny = FEnumerateEventsCompleteDelegate::CreateSP(this, &SUTReplayWindow::FlagDenyEnumerated);
		FEnumerateEventsCompleteDelegate EnumMultiKills = FEnumerateEventsCompleteDelegate::CreateSP(this, &SUTReplayWindow::MultiKillsEnumerated);
		FEnumerateEventsCompleteDelegate EnumSpreeKills = FEnumerateEventsCompleteDelegate::CreateSP(this, &SUTReplayWindow::SpreeKillsEnumerated);
		DemoNetDriver->EnumerateEvents(TEXT("Kills"), EnumKills);
		DemoNetDriver->EnumerateEvents(TEXT("FlagCaps"), EnumFlagCaps);
		DemoNetDriver->EnumerateEvents(TEXT("FlagReturns"), EnumFlagReturns);
		DemoNetDriver->EnumerateEvents(TEXT("FlagDeny"), EnumFlagDeny);
		DemoNetDriver->EnumerateEvents(TEXT("MultiKills"), EnumMultiKills);
		DemoNetDriver->EnumerateEvents(TEXT("SpreeKills"), EnumSpreeKills);
	}
}

void SUTReplayWindow::ParseJsonIntoBookmarkArray(const FString& JsonString, TArray<FBookmarkEvent>& BookmarkArray)
{
	//UE_LOG(UT, Log, TEXT("%s"), *JsonString);
	BookmarkArray.Empty();

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(JsonString);

	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Events;
		if (JsonObject->TryGetArrayField(TEXT("events"), Events))
		{
			for (int32 EventsIdx = 0; EventsIdx < Events->Num(); EventsIdx++)
			{
				FBookmarkEvent BookmarkEvent;
				const TSharedPtr<FJsonValue>& EventJsonValue = (*Events)[EventsIdx];
				EventJsonValue->AsObject()->TryGetStringField(TEXT("id"), BookmarkEvent.id);
				EventJsonValue->AsObject()->TryGetStringField(TEXT("meta"), BookmarkEvent.meta);
				int32 TimeTemp;
				EventJsonValue->AsObject()->TryGetNumberField(TEXT("time1"), TimeTemp);

				// time was in ms, should be in seconds
				BookmarkEvent.time = TimeTemp / 1000.0f;

				BookmarkArray.Add(BookmarkEvent);
			}
		}
	}
}

void SUTReplayWindow::KillsEnumerated(const FString& JsonString, bool bSucceeded)
{
	if (bSucceeded)
	{
		ParseJsonIntoBookmarkArray(JsonString, KillEvents);
	}
}


void SUTReplayWindow::FlagCapsEnumerated(const FString& JsonString, bool bSucceeded)
{
	if (bSucceeded)
	{
		ParseJsonIntoBookmarkArray(JsonString, FlagCapEvents);
	}
}

void SUTReplayWindow::FlagReturnsEnumerated(const FString& JsonString, bool bSucceeded)
{
	if (bSucceeded)
	{
		ParseJsonIntoBookmarkArray(JsonString, FlagReturnEvents);
	}
}

void SUTReplayWindow::FlagDenyEnumerated(const FString& JsonString, bool bSucceeded)
{
	if (bSucceeded)
	{
		ParseJsonIntoBookmarkArray(JsonString, FlagDenyEvents);
	}
}

void SUTReplayWindow::MultiKillsEnumerated(const FString& JsonString, bool bSucceeded)
{
	if (bSucceeded)
	{
		ParseJsonIntoBookmarkArray(JsonString, MultiKillEvents);
	}
}

void SUTReplayWindow::SpreeKillsEnumerated(const FString& JsonString, bool bSucceeded)
{
	if (bSucceeded)
	{
		ParseJsonIntoBookmarkArray(JsonString, SpreeKillEvents);
	}

	RefreshBookmarksComboBox();
}

FText SUTReplayWindow::GetTimeText() const
{
	if (DemoNetDriver.IsValid())
	{
		FFormatNamedArguments Args;
		Args.Add("TotalTime", FText::AsTimespan(FTimespan(0, 0, static_cast<int32>(DemoNetDriver->DemoTotalTime))));
		Args.Add("CurrentTime", FText::AsTimespan(FTimespan(0, 0, static_cast<int32>(DemoNetDriver->DemoCurrentTime))));
		return FText::Format(FText::FromString(TEXT("{CurrentTime} / {TotalTime}")), Args);
	}
	return FText();
}

void SUTReplayWindow::OnSetTimeSlider(float NewValue)
{
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (UTPC != nullptr)
	{
		// Unpause if we're paused
		if (DemoNetDriver.IsValid())
		{
			AWorldSettings* const WorldSettings = UTPC->GetWorldSettings();

			if (WorldSettings->Pauser != nullptr)
			{
				WorldSettings->Pauser = nullptr;
			}
		}

		UTPC->DemoGoTo(DemoNetDriver->DemoTotalTime * NewValue);
	}
}

float SUTReplayWindow::GetTimeSlider() const
{
	float SliderPos = 0.f;
	if (DemoNetDriver.IsValid() && DemoNetDriver->DemoTotalTime > 0.0f)
	{
		SliderPos = DemoNetDriver->DemoCurrentTime / DemoNetDriver->DemoTotalTime;
	}
	return SliderPos;
}

float SUTReplayWindow::GetTimeSliderLength() const
{
	if (DemoNetDriver.IsValid() && DemoNetDriver->DemoTotalTime > 0.0f)
	{
		return DemoNetDriver->DemoTotalTime;
	}

	return 1.0f;
}

void SUTReplayWindow::OnSetSpeedSlider(float NewValue)
{
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (UTPC != nullptr)
	{
		UTPC->DemoSetTimeDilation(NewValue);
	}
}

TOptional<float> SUTReplayWindow::GetSpeedSlider() const
{
	return PlayerOwner->GetWorld()->GetWorldSettings()->DemoPlayTimeDilation;
}

const FSlateBrush* SUTReplayWindow::GetPlayButtonBrush() const
{
	return PlayerOwner->GetWorld()->GetWorldSettings()->Pauser ? SUWindowsStyle::Get().GetBrush("UT.Replay.Button.Play") : SUWindowsStyle::Get().GetBrush("UT.Replay.Button.Pause");
}

FReply SUTReplayWindow::OnPlayPauseButtonClicked()
{
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (UTPC != nullptr)
	{
		UTPC->DemoPause();
	}
	return FReply::Handled();
}

FLinearColor SUTReplayWindow::GetTimeBarColor() const
{
	return FLinearColor(1.0f, 1.0f, 1.0f, FMath::Max(1.0f + HideTimeBarTime, 0.0f));
}

FSlateColor SUTReplayWindow::GetTimeBarBorderColor() const
{
	return FSlateColor(GetTimeBarColor());
}

void SUTReplayWindow::Tick(const FGeometry & AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	//Check if the mouse is close to the bottom of the screen to unhide the timebar
	UUTGameViewportClient* GVC = Cast<UUTGameViewportClient>(PlayerOwner->ViewportClient);
	if (GVC && GVC->GetGameViewport())
	{
		FVector2D MousePosition;
		FVector2D ScreenSize;
		GVC->GetMousePosition(MousePosition);
		GVC->GetViewportSize(ScreenSize);
		
		if (!GVC->GetGameViewport()->HasMouseCapture() && (MousePosition.Y / ScreenSize.Y > 0.8f))
		{
			HideTimeBarTime = 2.0f;
		}
	}
	HideTimeBarTime -= InDeltaTime;

	// If focus has changed and we're looking at kill bookmarks, refilter the bookmarks
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (UTPC &&	LastSpectedPlayerID != UTPC->LastSpectatedPlayerId)
	{
		LastSpectedPlayerID = UTPC->LastSpectatedPlayerId;
		if (SelectedBookmark->GetText().ToString() == TEXT("Kills"))
		{
			OnKillBookmarksSelected();
		}
		else if (SelectedBookmark->GetText().ToString() == TEXT("Multi Kills"))
		{
			OnMultiKillBookmarksSelected();
		}
		else if (SelectedBookmark->GetText().ToString() == TEXT("Spree Kills"))
		{
			OnSpreeKillBookmarksSelected();
		}
		else if (SelectedBookmark->GetText().ToString() == TEXT("Flag Returns"))
		{
			OnFlagReturnsBookmarksSelected();
		}
	}

	return SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

// @Returns true if the mouse position is inside the viewport
bool SUTReplayWindow::GetGameMousePosition(FVector2D& MousePosition) const
{
	// We need to get the mouse input but the mouse event only has the mouse in screen space.  We need it in viewport space and there
	// isn't a good way to get there.  So we punt and just get it from the game viewport.

	UUTGameViewportClient* GVC = Cast<UUTGameViewportClient>(PlayerOwner->ViewportClient);
	if (GVC)
	{
		return GVC->GetMousePosition(MousePosition);
	}
	return false;
}

FReply SUTReplayWindow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FGeometry TimeBarGeometry = FindChildGeometry(MyGeometry, TimeBar.ToSharedRef());
	if (TimeBarGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}
	return MouseClickHUD() ? FReply::Handled() : FReply::Unhandled();
}

FReply SUTReplayWindow::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return MouseClickHUD() ? FReply::Handled() : FReply::Unhandled();
}

bool SUTReplayWindow::MouseClickHUD()
{
	if (GetVis() != EVisibility::Hidden)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
		if (PC && PC->MyUTHUD)
		{
			FVector2D MousePosition;
			if (GetGameMousePosition(MousePosition))
			{
				UUTHUDWidget_SpectatorSlideOut* SpectatorWidget = PC->MyUTHUD->GetSpectatorSlideOut();
				if (SpectatorWidget)
				{
					SpectatorWidget->SetMouseInteractive(true);
					return SpectatorWidget->MouseClick(MousePosition);
				}
			}
		}
	}
	return false;
}

FReply SUTReplayWindow::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		FVector2D MousePosition;
		if (GetGameMousePosition(MousePosition))
		{
			UUTHUDWidget_SpectatorSlideOut* SpectatorWidget = PC->MyUTHUD->GetSpectatorSlideOut();
			if (SpectatorWidget)
			{
				SpectatorWidget->SetMouseInteractive(true);
				SpectatorWidget->TrackMouseMovement(MousePosition);
			}
		}
	}

	//Update the tooltip if the mouse is over the slider
	if (GetVis() != EVisibility::Hidden)
	{
		const FGeometry TimeSliderGeometry = FindChildGeometry(MyGeometry, TimeSlider.ToSharedRef());
		bDrawTooltip = TimeSliderGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition());
		if (bDrawTooltip)
		{
			float Alpha = TimeSlider->PositionToValue(TimeSliderGeometry, MouseEvent.GetScreenSpacePosition());
			if (DemoNetDriver.IsValid())
			{
				TooltipTime = DemoNetDriver->DemoTotalTime * Alpha;
			}

			//need to scale the position like we are doing for the whole widget
			float Scale = MyGeometry.Size.X / 1920.0f;
			ToolTipPos.X = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X / Scale;
			ToolTipPos.Y = MyGeometry.AbsoluteToLocal(TimeSliderGeometry.LocalToAbsolute(FVector2D(0.0f, 0.0f))).Y / Scale;
		}
	}

	return FReply::Unhandled();
}

FReply SUTReplayWindow::OnMarkRecordStartClicked()
{
	if (DemoNetDriver.IsValid())
	{
		RecordTimeStart = DemoNetDriver->DemoCurrentTime;
		TimeSlider->SetMarkStart(TimeSlider->GetValue());
		// Verify that time stop is in a valid spot
		if (RecordTimeStop > 0 && RecordTimeStop < RecordTimeStart)
		{
			RecordTimeStop = -1;
			TimeSlider->SetMarkEnd(-1);
		}
	}

	return FReply::Handled();
}

FReply SUTReplayWindow::OnMarkRecordStopClicked()
{
	if (DemoNetDriver.IsValid())
	{
		// Don't allow demo clips longer than 90 seconds right now, video temp files get really close to 4 gigs at that size
		if (RecordTimeStart > 0 && DemoNetDriver->DemoCurrentTime > RecordTimeStart && DemoNetDriver->DemoCurrentTime - RecordTimeStart < 90)
		{
			RecordTimeStop = DemoNetDriver->DemoCurrentTime;
			TimeSlider->SetMarkEnd(TimeSlider->GetValue());
		}
	}

	return FReply::Handled();
}

FReply SUTReplayWindow::OnRecordButtonClicked()
{
	if (DemoNetDriver.IsValid())
	{
		if (RecordTimeStart > 0 && RecordTimeStop > 0)
		{
			AWorldSettings* const WorldSettings = PlayerOwner->GetWorld()->GetWorldSettings();
			WorldSettings->Pauser = nullptr;

			PlayerOwner->GetWorld()->GetWorldSettings()->DemoPlayTimeDilation = 1.0f;
			DemoNetDriver->GotoTimeInSeconds(RecordTimeStart, FOnGotoTimeDelegate::CreateSP(this, &SUTReplayWindow::RecordSeekCompleted));
		}
	}

	return FReply::Handled();
}

FReply SUTReplayWindow::OnScreenshotButtonClicked()
{
	GScreenshotResolutionX = GetPlayerOwner()->GetProfileSettings()->ReplayScreenshotResX;
	GScreenshotResolutionY = GetPlayerOwner()->GetProfileSettings()->ReplayScreenshotResY;

	if (GScreenshotResolutionX <= 0)
	{
		// 4k resolution isn't actually >= 4000
		GScreenshotResolutionX = 3840;
		GScreenshotResolutionY = 2160;
	}

	PlayerOwner->ViewportClient->GetGameViewport()->TakeHighResScreenShot();

	return FReply::Handled();
}

FReply SUTReplayWindow::OnScreenshotConfigButtonClicked()
{
	FDialogResultDelegate Callback;
	Callback.BindRaw(this, &SUTReplayWindow::ScreenshotConfigResult);

	TSharedPtr<SUWDialog> NewDialog; // important to make sure the ref count stays until OpenDialog()
	SAssignNew(NewDialog, SUWScreenshotConfigDialog)
		.PlayerOwner(PlayerOwner)
		.ButtonMask(UTDIALOG_BUTTON_OK)
		.OnDialogResult(Callback);

	PlayerOwner->OpenDialog(NewDialog.ToSharedRef());

	return FReply::Handled();
}

void SUTReplayWindow::ScreenshotConfigResult(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonID)
{
}

void SUTReplayWindow::RecordSeekCompleted(bool bSucceeded)
{
	if (bSucceeded)
	{
		PlayerOwner->RecordReplay(RecordTimeStop - RecordTimeStart);
	}
	else
	{
		RecordTimeStart = -1;
		RecordTimeStop = -1;
		TimeSlider->SetMarkStart(-1);
		TimeSlider->SetMarkEnd(-1);
	}
}

FVector2D SUTReplayWindow::GetTimeTooltipPosition() const
{
	return ToolTipPos;
}

FVector2D SUTReplayWindow::GetTimeTooltipSize() const
{
	//TODO: Do a fancy interp when we have more to put in this tooltip other than the time (game events)
	return bDrawTooltip ? FVector2D(110.0f, 55.0f) : FVector2D(0.0f, 0.0f);
}

FText SUTReplayWindow::GetTooltipText() const
{
	return FText::AsTimespan(FTimespan(0, 0, static_cast<int32>(TooltipTime)));
}

EVisibility SUTReplayWindow::GetVis() const
{
	return PlayerOwner->AreMenusOpen() ? EVisibility::Hidden : EVisibility::Visible;
}

void SUTReplayWindow::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if (ArrangedChildren.Accepts(ChildVisibility))
	{
		FVector2D DesiredSize = FVector2D(1920.0f, 1080.0f);
		FVector2D ActualGeometrySize = AllottedGeometry.Size * AllottedGeometry.Scale;
		float Scale = 1.0f;
		if (AllottedGeometry.Size != DesiredSize)
		{
			//Scale to fit the width of the screen
			Scale = AllottedGeometry.Size.X / DesiredSize.X;
			DesiredSize = ActualGeometrySize / Scale;
		}
		ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(ChildSlot.GetWidget(), FVector2D(0.0f, 0.0f), DesiredSize, Scale));
	}
}

void SUTReplayWindow::OnKillBookmarksSelected()
{
	TArray<FBookmarkTimeAndColor> Bookmarks;

	if (KillEvents.Num() == 0)
	{
		TimeSlider->SetBookmarks(Bookmarks);
		return;
	}

	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	LastSpectedPlayerID = UTPC->LastSpectatedPlayerId;
	
	FString FilterID = GetSpectatedPlayerID();

	FBookmarkTimeAndColor TimeAndColor;
	TimeAndColor.Color = FLinearColor::Red;
	for (int32 EventsIdx = 0; EventsIdx < KillEvents.Num(); EventsIdx++)
	{
		if (!FilterID.IsEmpty() && FilterID != KillEvents[EventsIdx].meta)
		{
			continue;
		}

		TimeAndColor.Time = KillEvents[EventsIdx].time;
		Bookmarks.Add(TimeAndColor);
	}
	TimeSlider->SetBookmarks(Bookmarks);
}

void SUTReplayWindow::OnMultiKillBookmarksSelected()
{
	TArray<FBookmarkTimeAndColor> Bookmarks;

	if (MultiKillEvents.Num() == 0)
	{
		TimeSlider->SetBookmarks(Bookmarks);
		return;
	}

	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	LastSpectedPlayerID = UTPC->LastSpectatedPlayerId;
	
	FString FilterID = GetSpectatedPlayerID();

	FBookmarkTimeAndColor TimeAndColor;
	TimeAndColor.Color = FLinearColor::Yellow;
	for (int32 EventsIdx = 0; EventsIdx < MultiKillEvents.Num(); EventsIdx++)
	{
		if (!FilterID.IsEmpty() && FilterID != MultiKillEvents[EventsIdx].meta)
		{
			continue;
		}

		TimeAndColor.Time = MultiKillEvents[EventsIdx].time;
		Bookmarks.Add(TimeAndColor);
	}
	TimeSlider->SetBookmarks(Bookmarks);
}

FString SUTReplayWindow::GetSpectatedPlayerID()
{
	if (LastSpectedPlayerID > 0)
	{
		AUTPlayerController* UTPC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
		if (UTPC->GetWorld()->GameState)
		{
			const TArray<APlayerState*>& PlayerArray = UTPC->GetWorld()->GameState->PlayerArray;
			for (int32 i = 0; i < PlayerArray.Num(); i++)
			{
				AUTPlayerState* UTPS = Cast<AUTPlayerState>(PlayerArray[i]);
				if (UTPS)
				{
					if (UTPS->SpectatingID == LastSpectedPlayerID)
					{
						// bots don't have StatsID, they just use playername right now
						if (!UTPS->StatsID.IsEmpty())
						{
							return UTPS->StatsID;
						}
						else
						{
							return UTPS->PlayerName;
						}
					}
				}
			}
		}
	}

	return TEXT("");
}

void SUTReplayWindow::OnSpreeKillBookmarksSelected()
{
	TArray<FBookmarkTimeAndColor> Bookmarks;

	if (SpreeKillEvents.Num() == 0)
	{
		TimeSlider->SetBookmarks(Bookmarks);
		return;
	}

	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	LastSpectedPlayerID = UTPC->LastSpectatedPlayerId;

	FString FilterID = GetSpectatedPlayerID();

	FBookmarkTimeAndColor TimeAndColor;
	TimeAndColor.Color = FLinearColor::Yellow;
	for (int32 EventsIdx = 0; EventsIdx < SpreeKillEvents.Num(); EventsIdx++)
	{
		if (!FilterID.IsEmpty() && FilterID != SpreeKillEvents[EventsIdx].meta)
		{
			continue;
		}

		TimeAndColor.Time = SpreeKillEvents[EventsIdx].time;
		Bookmarks.Add(TimeAndColor);
	}
	TimeSlider->SetBookmarks(Bookmarks);
}

void SUTReplayWindow::OnFlagReturnsBookmarksSelected()
{
	TArray<FBookmarkTimeAndColor> Bookmarks;

	if (FlagReturnEvents.Num() == 0)
	{
		TimeSlider->SetBookmarks(Bookmarks);
		return;
	}

	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	LastSpectedPlayerID = UTPC->LastSpectatedPlayerId;

	FString FilterID = GetSpectatedPlayerID();

	FBookmarkTimeAndColor TimeAndColor;
	TimeAndColor.Color = FLinearColor::Yellow;
	for (int32 EventsIdx = 0; EventsIdx < FlagReturnEvents.Num(); EventsIdx++)
	{
		if (!FilterID.IsEmpty() && FilterID != FlagReturnEvents[EventsIdx].meta)
		{
			continue;
		}

		TimeAndColor.Time = FlagReturnEvents[EventsIdx].time;
		Bookmarks.Add(TimeAndColor);
	}
	TimeSlider->SetBookmarks(Bookmarks);
}

void SUTReplayWindow::OnBookmarkSetSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	TArray<FBookmarkTimeAndColor> Bookmarks;
	FBookmarkTimeAndColor TimeAndColor;
	SelectedBookmark->SetText(FText::FromString(*NewSelection));
	if (*NewSelection == TEXT("Bookmarks"))
	{
		// clear bookmarks
		TimeSlider->SetBookmarks(Bookmarks);
	}
	else if (*NewSelection == TEXT("Kills"))
	{
		OnKillBookmarksSelected();
	}
	else if (*NewSelection == TEXT("Flag Captures"))
	{
		for (int32 EventsIdx = 0; EventsIdx < FlagCapEvents.Num(); EventsIdx++)
		{
			if (FlagCapEvents[EventsIdx].meta == TEXT("0"))
			{
				TimeAndColor.Color = FLinearColor::Red;
			}
			else
			{
				TimeAndColor.Color = FLinearColor::Blue;
			}
			TimeAndColor.Time = FlagCapEvents[EventsIdx].time;
			Bookmarks.Add(TimeAndColor);
		}
		TimeSlider->SetBookmarks(Bookmarks);
	}
	else if (*NewSelection == TEXT("Flag Denies"))
	{
		for (int32 EventsIdx = 0; EventsIdx < FlagDenyEvents.Num(); EventsIdx++)
		{
			if (FlagDenyEvents[EventsIdx].meta == TEXT("0"))
			{
				TimeAndColor.Color = FLinearColor::Red;
			}
			else
			{
				TimeAndColor.Color = FLinearColor::Blue;
			}
			TimeAndColor.Time = FlagDenyEvents[EventsIdx].time;
			Bookmarks.Add(TimeAndColor);
		}
		TimeSlider->SetBookmarks(Bookmarks);
	}
	else if (*NewSelection == TEXT("Flag Returns"))
	{
		OnFlagReturnsBookmarksSelected();
	}
	else if (*NewSelection == TEXT("Multi Kills"))
	{
		OnMultiKillBookmarksSelected();
	}
	else if (*NewSelection == TEXT("Spree Kills"))
	{
		OnSpreeKillBookmarksSelected();
	}
}

void SUTReplayWindow::RefreshBookmarksComboBox()
{
	BookmarkNameList.Empty();
	TSharedPtr<FString> DefaultVariant = MakeShareable(new FString(TEXT("Bookmarks")));
	BookmarkNameList.Add(DefaultVariant);
	BookmarksComboBox->SetSelectedItem(DefaultVariant);

	if (KillEvents.Num())
	{
		TSharedPtr<FString> BookmarkName = MakeShareable(new FString(TEXT("Kills")));
		BookmarkNameList.Add(BookmarkName);
	}
	if (FlagCapEvents.Num())
	{
		TSharedPtr<FString> BookmarkName = MakeShareable(new FString(TEXT("Flag Captures")));
		BookmarkNameList.Add(BookmarkName);
	}
	if (FlagDenyEvents.Num())
	{
		TSharedPtr<FString> BookmarkName = MakeShareable(new FString(TEXT("Flag Denies")));
		BookmarkNameList.Add(BookmarkName);
	}
	if (FlagReturnEvents.Num())
	{
		TSharedPtr<FString> BookmarkName = MakeShareable(new FString(TEXT("Flag Returns")));
		BookmarkNameList.Add(BookmarkName);
	}
	if (MultiKillEvents.Num())
	{
		TSharedPtr<FString> BookmarkName = MakeShareable(new FString(TEXT("Multi Kills")));
		BookmarkNameList.Add(BookmarkName);
	}
	if (SpreeKillEvents.Num())
	{
		TSharedPtr<FString> BookmarkName = MakeShareable(new FString(TEXT("Spree Kills")));
		BookmarkNameList.Add(BookmarkName);
	}
}

TSharedRef<SWidget> SUTReplayWindow::GenerateStringListWidget(TSharedPtr<FString> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(FText::FromString(*InItem.Get()))
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
		];
}

#endif