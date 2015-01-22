// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SReflectorToolTipWidget"


class SReflectorToolTipWidget
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SReflectorToolTipWidget)
		: _WidgetInfoToVisualize()
	{ }

		SLATE_ARGUMENT(TSharedPtr<FReflectorNode>, WidgetInfoToVisualize)

	SLATE_END_ARGS()

public:

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs )
	{
		this->WidgetInfo = InArgs._WidgetInfoToVisualize;

		ChildSlot
		[
			SNew(SGridPanel)
				.FillColumn(1, 1.0f)

			// Desired Size
			+ SGridPanel::Slot(0, 0)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("DesiredSize", "Desired Size"))
				]

			// Desired Size Value
			+ SGridPanel::Slot(1, 0)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(this, &SReflectorToolTipWidget::GetWidgetsDesiredSize)
				]

			// Actual Size
			+ SGridPanel::Slot(0, 1)
				[
					SNew( STextBlock )
						.Text(LOCTEXT("ActualSize", "Actual Size"))
				]

			// Actual Size Value
			+ SGridPanel::Slot(1, 1)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(this, &SReflectorToolTipWidget::GetWidgetActualSize)
				]

			// Size Info
			+ SGridPanel::Slot(0, 2)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("SizeInfo", "Size Info"))
				]

			// Size Info Value
			+ SGridPanel::Slot(1, 2)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(this, &SReflectorToolTipWidget::GetSizeInfo)
				]

			// Enabled
			+ SGridPanel::Slot(0, 3)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("Enabled", "Enabled"))
				]

			// Enabled Value
			+ SGridPanel::Slot(1, 3)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(this, &SReflectorToolTipWidget::GetEnabled)
				]
		];
	}

private:

	FString GetWidgetsDesiredSize() const
	{
		TSharedPtr<SWidget> TheWidget = WidgetInfo.Get()->Widget.Pin();

		return TheWidget.IsValid()
			? TheWidget->GetDesiredSize().ToString()
			: FString();
	}

	FString GetWidgetActualSize() const
	{
		return WidgetInfo.Get()->Geometry.Size.ToString();
	}

	FString GetSizeInfo() const
	{
		TSharedPtr<SWidget> TheWidget = WidgetInfo.Get()->Widget.Pin();

		return TheWidget.IsValid()
			? WidgetInfo.Get()->Geometry.ToString()
			: FString();
	}

	FString GetEnabled() const
	{
		TSharedPtr<SWidget> TheWidget = WidgetInfo.Get()->Widget.Pin();

		return TheWidget.IsValid()
			? (TheWidget->IsEnabled() ? "True" : "False")
			: FString();
	}

private:

	/** The info about the widget that we are visualizing. */
	TAttribute<TSharedPtr<FReflectorNode>> WidgetInfo;
};


#undef LOCTEXT_NAMESPACE
