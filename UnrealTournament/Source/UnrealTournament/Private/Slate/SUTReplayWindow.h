// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTReplayWindow : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUTReplayWindow)
	{}

	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)
	SLATE_ARGUMENT(TWeakObjectPtr<class UDemoNetDriver>, DemoNetDriver)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	inline TWeakObjectPtr<class UUTLocalPlayer> GetPlayerOwner()
	{
		return PlayerOwner;
	}

	inline TWeakObjectPtr<class UDemoNetDriver> GetDemoNetDriver()
	{
		return DemoNetDriver;
	}

	virtual void Tick(const FGeometry & AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:

	float RecordTimeStart;
	float RecordTimeStop;
	FReply OnMarkRecordStartClicked();
	FReply OnMarkRecordStopClicked();
	FReply OnRecordButtonClicked();
	void RecordSeekCompleted(bool bSucceeded);

	TSharedPtr<class SUTProgressSlider> TimeSlider;
	TSharedPtr<class SButton> RecordButton;
	TSharedPtr<class SButton> MarkStartButton;
	TSharedPtr<class SButton> MarkEndButton;
	TSharedPtr<class SBorder> TimeBar;

	//Time remaining to auto hide the time bar
	float HideTimeBarTime;
	FLinearColor GetTimeBarColor() const;
	FSlateColor GetTimeBarBorderColor() const;

	void OnSetTimeSlider(float NewValue);
	float GetTimeSlider() const;

	void OnSetSpeedSlider(float NewValue);
	TOptional<float> GetSpeedSlider() const;

	float SpeedMin;
	float SpeedMax;

	FText GetTimeText() const;
	
	const FSlateBrush* GetPlayButtonBrush() const;
	FReply OnPlayPauseButtonClicked();

	//Mouse interaction overrides so it can send input to UUTHUDWidget_SpectatorSlideOut
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	bool GetGameMousePosition(FVector2D& MousePosition) const;
	virtual bool MouseClickHUD();

	bool bDrawTooltip;
	float TooltipTime;
	FVector2D ToolTipPos;

	FText GetTooltipText() const;
	FVector2D GetTimeTooltipSize() const;
	FVector2D GetTimeTooltipPosition() const;

private:
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
	TWeakObjectPtr<class UDemoNetDriver> DemoNetDriver;
};

#endif
