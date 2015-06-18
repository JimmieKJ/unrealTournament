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

#if !UE_SERVER

void SUTReplayWindow::Construct(const FArguments& InArgs)
{
	SpeedMin = 0.1f;
	SpeedMax = 2.0f;

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

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SCanvas)

			//Time Controls
			+ SCanvas::Slot()
			.Position(TimePos)
			.Size(TimeSize)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					//Background image
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UT.TopMenu.Shadow"))
				]
				+ SOverlay::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.Padding(10.0f)
				[
					//The Main time slider
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.AutoHeight()
					.Padding(0.0f,0.0f,0.0f,10.0f)
					[
						SNew(SUTProgressSlider)
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
							SNew(STextBlock)
							.Text(this, &SUTReplayWindow::GetTimeText)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
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
	];
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
	if (DemoNetDriver.IsValid())
	{
		DemoNetDriver->GotoTimeInSeconds(DemoNetDriver->DemoTotalTime * NewValue);
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
	PlayerOwner->GetWorld()->GetWorldSettings()->DemoPlayTimeDilation = NewValue;
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
	AWorldSettings* const WorldSettings = PlayerOwner->GetWorld()->GetWorldSettings();
	if (WorldSettings->Pauser == nullptr)
	{
		WorldSettings->Pauser = (PlayerOwner->PlayerController != nullptr) ? PlayerOwner->PlayerController->PlayerState : nullptr;
	}
	else
	{
		WorldSettings->Pauser = nullptr;
	}
	return FReply::Handled();
}

void SUTReplayWindow::Tick(const FGeometry & AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	return SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

// @Returns true if the mouse position is inside the viewport
bool SUTReplayWindow::GetGameMousePosition(FVector2D& MousePosition)
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

	return FReply::Unhandled();
}



#endif