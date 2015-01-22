// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SMessagingBreakpointsTableRow"


/**
 * Implements a row widget for the session console log.
 */
class SMessagingBreakpointsTableRow
	: public SMultiColumnTableRow<IMessageTracerBreakpointPtr>
{
public:

	SLATE_BEGIN_ARGS(SMessagingBreakpointsTableRow) { }
		SLATE_ARGUMENT(IMessageTracerBreakpointPtr, Breakpoint)
		SLATE_ARGUMENT(TSharedPtr<ISlateStyle>, Style)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InOwnerTableView The table view that owns this row.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		check(InArgs._Breakpoint.IsValid());
		check(InArgs._Style.IsValid());

		Breakpoint = InArgs._Breakpoint;
		Style = InArgs._Style;

		SMultiColumnTableRow<IMessageTracerBreakpointPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

public:

	// SMultiColumnTableRow interface

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		if (ColumnName == "HitCount")
		{
			return SNullWidget::NullWidget;
		}
		else if (ColumnName == "Name")
		{
			return SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					[
						SNew(SCheckBox)
							.IsChecked(ECheckBoxState::Checked)
					]

				+ SHorizontalBox::Slot()
					.Padding(FMargin(4.0f, 0.0f))
					[
						SNew(SImage)
							.Image(Style->GetBrush("BreakDisabled"))
					]

				+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("TempNameColumn", "@todo"))
					];
		}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:

	/** Holds a pointer to the breakpoint that is shown in this row. */
	IMessageTracerBreakpointPtr Breakpoint;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;
};


#undef LOCTEXT_NAMESPACE
