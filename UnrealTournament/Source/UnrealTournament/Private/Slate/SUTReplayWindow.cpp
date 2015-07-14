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
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "UTVideoRecordingFeature.h"
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
										SAssignNew(RecordButton, SButton)
										.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
										.OnClicked(this, &SUTReplayWindow::OnMarkRecordStartClicked)
										.ContentPadding(1.0f)
										.Content()
										[
											SNew(SImage)
											.Image(SUWindowsStyle::Get().GetBrush("UT.Replay.Button.MarkStart"))
										]
									]
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Left)
									.AutoWidth()
									[
										SAssignNew(MarkStartButton, SButton)
										.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
										.OnClicked(this, &SUTReplayWindow::OnMarkRecordStopClicked)
										.ContentPadding(1.0f)
										.Content()
										[
											SNew(SImage)
											.Image(SUWindowsStyle::Get().GetBrush("UT.Replay.Button.MarkEnd"))
										]
									]
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Left)
									.AutoWidth()
									[
										SAssignNew(MarkEndButton, SButton)
										.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
										.OnClicked(this, &SUTReplayWindow::OnRecordButtonClicked)
										.ContentPadding(1.0f)
										.Content()
										[
											SNew(SImage)
											.Image(SUWindowsStyle::Get().GetBrush("UT.Replay.Button.Record"))
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
	return MouseClickHUD() ? FReply::Handled() : FReply::Unhandled();
}

FReply SUTReplayWindow::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return MouseClickHUD() ? FReply::Handled() : FReply::Unhandled();
}

bool SUTReplayWindow::MouseClickHUD()
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
	const FGeometry TimeSliderGeometry = FindChildGeometry(MyGeometry, TimeSlider.ToSharedRef());
	bDrawTooltip = TimeSliderGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition());
	if (bDrawTooltip)
	{
		float Alpha = TimeSlider->PositionToValue(TimeSliderGeometry, MouseEvent.GetScreenSpacePosition());
		TooltipTime = DemoNetDriver->DemoTotalTime * Alpha;

		ToolTipPos.X = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X;
		ToolTipPos.Y = MyGeometry.AbsoluteToLocal(TimeSliderGeometry.LocalToAbsolute(FVector2D(0.0f, 0.0f))).Y;
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
			DemoNetDriver->GotoTimeInSeconds(RecordTimeStart, FOnGotoTimeDelegate::CreateRaw(this, &SUTReplayWindow::RecordSeekCompleted));
		}
	}

	return FReply::Handled();
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
	return PlayerOwner->AreMenusOpen() ? EVisibility::Hidden : EVisibility::SelfHitTestInvisible;
}

#endif