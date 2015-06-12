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

	//TODO: make custom widgets for these
	void OnSetTimeSlider(float NewValue);
	float GetTimeSlider() const;

	void OnSetSpeedSlider(float NewValue);
	TOptional<float> GetSpeedSlider() const;

	float SpeedMin;
	float SpeedMax;

	FText GetTimeText() const;

	const FSlateBrush* GetPlayButtonBrush() const;
	FReply OnPlayPauseButtonClicked();

private:
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
	TWeakObjectPtr<class UDemoNetDriver> DemoNetDriver;
};

#endif
