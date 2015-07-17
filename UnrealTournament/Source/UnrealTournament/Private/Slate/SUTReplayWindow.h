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
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;

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

	TSharedPtr< SComboBox< TSharedPtr<FString> > > BookmarksComboBox;
	TArray<TSharedPtr<FString>> BookmarkNameList;
	TSharedPtr<STextBlock> SelectedBookmark;
	void OnBookmarkSetSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void RefreshBookmarksComboBox();
	FString BookmarkFocusPlayer;
	/** utility to generate a simple text widget for list and combo boxes given a string value */
	TSharedRef<SWidget> GenerateStringListWidget(TSharedPtr<FString> InItem);

	//Time remaining to auto hide the time bar
	float HideTimeBarTime;
	FLinearColor GetTimeBarColor() const;
	FSlateColor GetTimeBarBorderColor() const;

	void OnSetTimeSlider(float NewValue);
	float GetTimeSlider() const;
	float GetTimeSliderLength() const;

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
	
	struct FBookmarkEvent
	{
		FString id;
		FString meta;
		float time;
	};

	void KillsEnumerated(const FString& JsonString, bool bSucceeded);
	void FlagCapsEnumerated(const FString& JsonString, bool bSucceeded);
	void FlagDenyEnumerated(const FString& JsonString, bool bSucceeded);
	void MultiKillsEnumerated(const FString& JsonString, bool bSucceeded);
	void ParseJsonIntoBookmarkArray(const FString& JsonString, TArray<FBookmarkEvent>& BookmarkArray);

	TArray<FBookmarkEvent> KillEvents;
	TArray<FBookmarkEvent> FlagCapEvents;
	TArray<FBookmarkEvent> FlagDenyEvents;
	TArray<FBookmarkEvent> MultiKillEvents;

	bool bDrawTooltip;
	float TooltipTime;
	FVector2D ToolTipPos;

	FText GetTooltipText() const;
	FVector2D GetTimeTooltipSize() const;
	FVector2D GetTimeTooltipPosition() const;

	EVisibility GetVis() const;

private:
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
	TWeakObjectPtr<class UDemoNetDriver> DemoNetDriver;
};

#endif
