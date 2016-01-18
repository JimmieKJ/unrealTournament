// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITimeSlider.h"

class STimeRangeSlider;

DECLARE_DELEGATE_RetVal( bool, STimeRangeGetter );

class STimeRange : public ITimeSlider, public TDefaultNumericTypeInterface<float>
{
public:
	SLATE_BEGIN_ARGS(STimeRange)
	{}
		/* If we should show frame numbers on the timeline */
		SLATE_ARGUMENT( TAttribute<bool>, ShowFrameNumbers )
		/* The time snap interval for the timeline */
		SLATE_ARGUMENT( TAttribute<float>, TimeSnapInterval )
	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 * 
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, TSharedRef<ITimeSliderController> InTimeSliderController );

	float GetTimeSnapInterval() const;

	using ITimeSlider::ToString;
	
protected:
	float InTime() const;
	float OutTime() const;
	float StartTime() const;
	float EndTime() const;

	TOptional<float> MinInTime() const;
	TOptional<float> MaxInTime() const;
	TOptional<float> MinOutTime() const;
	TOptional<float> MaxOutTime() const;
	TOptional<float> MaxStartTime() const;
	TOptional<float> MinEndTime() const;
	
	void OnStartTimeCommitted(float NewValue, ETextCommit::Type InTextCommit);
	void OnEndTimeCommitted(float NewValue, ETextCommit::Type InTextCommit);
	void OnInTimeCommitted(float NewValue, ETextCommit::Type InTextCommit);
	void OnOutTimeCommitted(float NewValue, ETextCommit::Type InTextCommit);

	void OnStartTimeChanged(float NewValue);
	void OnEndTimeChanged(float NewValue);
	void OnInTimeChanged(float NewValue);
	void OnOutTimeChanged(float NewValue);

	FText InTimeTooltip() const;
	FText OutTimeTooltip() const;
	FText StartTimeTooltip() const;
	FText EndTimeTooltip() const;

	/** Convert the type to/from a string */
	virtual FString ToString(const float& Value) const override;
	virtual TOptional<float> FromString(const FString& InString) override;

private:
	TSharedPtr<ITimeSliderController> TimeSliderController;
	TSharedPtr<STimeRangeSlider> TimeRangeSlider;
	TAttribute<bool> ShowFrameNumbers;
	TAttribute<float> TimeSnapInterval;
};