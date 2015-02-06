// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SMessagingAddressTableRow"


/**
 * Implements a row widget for the dispatch state list.
 */
class SMessagingAddressTableRow
	: public SMultiColumnTableRow<FMessageTracerAddressInfoPtr>
{
public:

	SLATE_BEGIN_ARGS(SMessagingAddressTableRow) { }
		SLATE_ARGUMENT(FMessageTracerAddressInfoPtr, AddressInfo)
		SLATE_ARGUMENT(TSharedPtr<ISlateStyle>, Style)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InOwnerTableView The table view that owns this row.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const FMessagingDebuggerModelRef& InModel )
	{
		check(InArgs._Style.IsValid());
		check(InArgs._AddressInfo.IsValid());

		AddressInfo = InArgs._AddressInfo;
		Model = InModel;
		Style = InArgs._Style;

		SMultiColumnTableRow<FMessageTracerAddressInfoPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

public:

	// SMultiColumnTableRow interface

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		static const FNumberFormattingOptions TimeRegisteredFormattingOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(5)
			.SetMaximumFractionalDigits(5);

		if (ColumnName == "Address")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::FromString(AddressInfo->Address.ToString()))
				];
		}
		else if (ColumnName == "TimeRegistered")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::AsNumber(AddressInfo->TimeRegistered - GStartTime, &TimeRegisteredFormattingOptions))
				];
		}
		else if (ColumnName == "TimeUnregistered")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text((AddressInfo->TimeUnregistered > 0.0) ? FText::AsNumber(AddressInfo->TimeUnregistered - GStartTime, &TimeRegisteredFormattingOptions) : LOCTEXT("Never", "Never"))
				];
		}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:

	/** Holds the address information. */
	FMessageTracerAddressInfoPtr AddressInfo;

	/** Holds a pointer to the view model. */
	FMessagingDebuggerModelPtr Model;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;
};


#undef LOCTEXT_NAMESPACE
