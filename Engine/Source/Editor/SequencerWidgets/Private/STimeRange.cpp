// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerWidgetsPrivatePCH.h"
#include "STimeRange.h"
#include "STimeRangeSlider.h"
#include "SlateStyle.h"
#include "EditorStyle.h"

#define LOCTEXT_NAMESPACE "STimeRange"

void STimeRange::Construct( const STimeRange::FArguments& InArgs, TSharedRef<ITimeSliderController> InTimeSliderController )
{
	TimeSliderController = InTimeSliderController;
	ShowFrameNumbers = InArgs._ShowFrameNumbers;
	TimeSnapInterval = InArgs._TimeSnapInterval;

	auto NumericTypeInterface = SharedThis(this);

	this->ChildSlot
	.HAlign(HAlign_Fill)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 2.0f)
		[
			SNew(SBox)
			.WidthOverride(36)
			.HAlign(HAlign_Center)
			[
				SNew(SSpinBox<float>)
				.Value(this, &STimeRange::StartTime)
				.ToolTipText(this, &STimeRange::StartTimeTooltip )
				.OnValueCommitted( this, &STimeRange::OnStartTimeCommitted )
				.OnValueChanged( this, &STimeRange::OnStartTimeChanged )
				.MinValue(TOptional<float>())
				.MaxValue(this, &STimeRange::MaxOutTime)
				.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
				.TypeInterface(NumericTypeInterface)
				.ClearKeyboardFocusOnCommit(true)
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 2.0f)
		[
			SNew(SBox)
			.WidthOverride(36)
			.HAlign(HAlign_Center)
			[
				SNew(SSpinBox<float>)
				.Value(this, &STimeRange::InTime)
				.ToolTipText(this, &STimeRange::InTimeTooltip)
				.OnValueCommitted( this, &STimeRange::OnInTimeCommitted )
				.OnValueChanged( this, &STimeRange::OnInTimeChanged )
				.MinValue(TOptional<float>())
				.MaxValue(this, &STimeRange::MaxOutTime)
				.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
				.TypeInterface(NumericTypeInterface)
				.ClearKeyboardFocusOnCommit(true)
			]
		]
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(2.0f, 4.0f)
		[
			SAssignNew(TimeRangeSlider, STimeRangeSlider, InTimeSliderController, SharedThis(this))
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 2.0f)
		[
			SNew(SBox)
			.WidthOverride(36)
			.HAlign(HAlign_Center)
			[
				SNew(SSpinBox<float>)
				.Value(this, &STimeRange::OutTime)
				.ToolTipText(this, &STimeRange::OutTimeTooltip)
				.OnValueCommitted( this, &STimeRange::OnOutTimeCommitted )
				.OnValueChanged( this, &STimeRange::OnOutTimeChanged )
				.MinValue(this, &STimeRange::MinInTime)
				.MaxValue(TOptional<float>())
				.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
				.TypeInterface(NumericTypeInterface)
				.ClearKeyboardFocusOnCommit(true)
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 2.0f)
		[
			SNew(SBox)
			.WidthOverride(36)
			.HAlign(HAlign_Center)
			[
				SNew(SSpinBox<float>)
				.Value(this, &STimeRange::EndTime)
				.ToolTipText(this, &STimeRange::EndTimeTooltip)
				.OnValueCommitted( this, &STimeRange::OnEndTimeCommitted )
				.OnValueChanged( this, &STimeRange::OnEndTimeChanged )
				.MinValue(this, &STimeRange::MinInTime)
				.MaxValue(TOptional<float>())
				.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
				.TypeInterface(NumericTypeInterface)
				.ClearKeyboardFocusOnCommit(true)
			]
		]
	];
}

float STimeRange::StartTime() const
{
	return TimeSliderController.IsValid() ? TimeSliderController.Get()->GetClampRange().GetLowerBoundValue() : 0.f;
}

float STimeRange::EndTime() const
{
	return TimeSliderController.IsValid() ? TimeSliderController.Get()->GetClampRange().GetUpperBoundValue() : 0.f;
}

float STimeRange::InTime() const
{
	return TimeSliderController.IsValid() ? TimeSliderController.Get()->GetViewRange().GetLowerBoundValue() : 0.f;
}

float STimeRange::OutTime() const
{
	return TimeSliderController.IsValid() ? TimeSliderController.Get()->GetViewRange().GetUpperBoundValue() : 0.f;
}

TOptional<float> STimeRange::MinInTime() const
{
	return StartTime();
}

TOptional<float> STimeRange::MaxInTime() const
{
	return OutTime();
}

TOptional<float> STimeRange::MinOutTime() const
{
	return InTime();
}

TOptional<float> STimeRange::MaxOutTime() const
{
	return EndTime();
}

TOptional<float> STimeRange::MaxStartTime() const
{
	return OutTime();
}

TOptional<float> STimeRange::MinEndTime() const
{
	return InTime();
}

void STimeRange::OnStartTimeCommitted(float NewValue, ETextCommit::Type InTextCommit)
{
	OnStartTimeChanged(NewValue);
}

void STimeRange::OnEndTimeCommitted(float NewValue, ETextCommit::Type InTextCommit)
{
	OnEndTimeChanged(NewValue);
}

void STimeRange::OnInTimeCommitted(float NewValue, ETextCommit::Type InTextCommit)
{
	OnInTimeChanged(NewValue);
}

void STimeRange::OnOutTimeCommitted(float NewValue, ETextCommit::Type InTextCommit)
{
	OnOutTimeChanged(NewValue);
}

void STimeRange::OnStartTimeChanged(float NewValue)
{
	if (auto* Controller = TimeSliderController.Get())
	{
		Controller->SetClampRange(NewValue, Controller->GetClampRange().GetUpperBoundValue());

		if (NewValue > Controller->GetViewRange().GetLowerBoundValue())
		{
			Controller->SetViewRange(NewValue, Controller->GetViewRange().GetUpperBoundValue(), EViewRangeInterpolation::Immediate);
		}
	}
}

void STimeRange::OnEndTimeChanged(float NewValue)
{
	if (auto* Controller = TimeSliderController.Get())
	{
		Controller->SetClampRange(Controller->GetClampRange().GetLowerBoundValue(), NewValue);

		if (NewValue < Controller->GetViewRange().GetUpperBoundValue())
		{
			Controller->SetViewRange(Controller->GetViewRange().GetLowerBoundValue(), NewValue, EViewRangeInterpolation::Immediate);
		}
	}
}

void STimeRange::OnInTimeChanged(float NewValue)
{
	if (auto* Controller = TimeSliderController.Get())
	{
		if (NewValue < TimeSliderController.Get()->GetClampRange().GetLowerBoundValue())
		{
			Controller->SetClampRange(NewValue, Controller->GetClampRange().GetUpperBoundValue());
		}

		Controller->SetViewRange(NewValue, Controller->GetViewRange().GetUpperBoundValue(), EViewRangeInterpolation::Immediate);
	}
}

void STimeRange::OnOutTimeChanged(float NewValue)
{
	if (auto* Controller = TimeSliderController.Get())
	{
		if (NewValue > Controller->GetClampRange().GetUpperBoundValue())
		{
			Controller->SetClampRange(Controller->GetClampRange().GetLowerBoundValue(), NewValue);
		}

		Controller->SetViewRange(Controller->GetViewRange().GetLowerBoundValue(), NewValue, EViewRangeInterpolation::Immediate);
	}
}

FText STimeRange::StartTimeTooltip() const
{
	bool bShowFrameNumbers = ShowFrameNumbers.IsBound() ? ShowFrameNumbers.Get() : false;
	if (bShowFrameNumbers)
	{
		return LOCTEXT("StartFrameTooltip", "Start frame");
	}
	else
	{
		return LOCTEXT("StartTimeTooltip", "Start time");
	}
}

FText STimeRange::InTimeTooltip() const
{
	bool bShowFrameNumbers = ShowFrameNumbers.IsBound() ? ShowFrameNumbers.Get() : false;
	if (bShowFrameNumbers)
	{
		return LOCTEXT("InFrameTooltip", "In frame");
	}
	else
	{
		return LOCTEXT("InTimeTooltip", "In time");
	}
}

FText STimeRange::OutTimeTooltip() const
{
	bool bShowFrameNumbers = ShowFrameNumbers.IsBound() ? ShowFrameNumbers.Get() : false;
	if (bShowFrameNumbers)
	{
		return LOCTEXT("OutFrameTooltip", "Out frame");
	}
	else
	{
		return LOCTEXT("OutTimeTooltip", "Out time");
	}
}

FText STimeRange::EndTimeTooltip() const
{
	bool bShowFrameNumbers = ShowFrameNumbers.IsBound() ? ShowFrameNumbers.Get() : false;
	if (bShowFrameNumbers)
	{
		return LOCTEXT("EndFrameTooltip", "End frame");
	}
	else
	{
		return LOCTEXT("EndTimeTooltip", "End time");
	}
}

float STimeRange::GetTimeSnapInterval() const
{
	if (TimeSnapInterval.IsBound())
	{
		return TimeSnapInterval.Get();
	}
	return 1.f;
}

FString STimeRange::ToString(const float& Value) const
{
	bool bShowFrameNumbers = ShowFrameNumbers.IsBound() ? ShowFrameNumbers.Get() : false;

	if (bShowFrameNumbers && TimeSliderController.IsValid())
	{
		int32 Frame = TimeSliderController.Get()->TimeToFrame(Value);
		return FString::Printf(TEXT("%d"), Frame);
	}

	return FString::Printf(TEXT("%.2f"), Value);
}

TOptional<float> STimeRange::FromString(const FString& InString)
{
	bool bShowFrameNumbers = ShowFrameNumbers.IsBound() ? ShowFrameNumbers.Get() : false;
	if (bShowFrameNumbers && TimeSliderController.IsValid())
	{
		int32 NewEndFrame = FCString::Atoi(*InString);
		return float(TimeSliderController.Get()->FrameToTime(NewEndFrame));
	}

	return FCString::Atof(*InString);
}



#undef LOCTEXT_NAMESPACE
