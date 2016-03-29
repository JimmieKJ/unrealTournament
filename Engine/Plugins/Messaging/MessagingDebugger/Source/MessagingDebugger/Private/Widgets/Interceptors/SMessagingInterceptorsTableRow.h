// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SMessagingInterceptorTableRow"


namespace MessagingInterceptorTableRow
{
	static const FNumberFormattingOptions TimeRegisteredFormattingOptions = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(5)
		.SetMaximumFractionalDigits(5);
}


/**
 * Implements a row widget for the interceptors list.
 */
class SMessagingInterceptorsTableRow
	: public SMultiColumnTableRow<FMessageTracerInterceptorInfoPtr>
{
public:

	SLATE_BEGIN_ARGS(SMessagingInterceptorsTableRow) { }
		SLATE_ARGUMENT(FMessageTracerInterceptorInfoPtr, InterceptorInfo)
		SLATE_ARGUMENT(TSharedPtr<ISlateStyle>, Style)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InOwnerTableView The table view that owns this row.
	 */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const FMessagingDebuggerModelRef& InModel)
	{
		check(InArgs._Style.IsValid());
		check(InArgs._InterceptorInfo.IsValid());

		InterceptorInfo = InArgs._InterceptorInfo;
		Model = InModel;
		Style = InArgs._Style;

		SMultiColumnTableRow<FMessageTracerInterceptorInfoPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

public:

	// SMultiColumnTableRow interface

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == "Name")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::FromString(InterceptorInfo->Name.ToString()))
				];
		}
		else if (ColumnName == "TimeRegistered")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::AsNumber(InterceptorInfo->TimeRegistered - GStartTime, &MessagingInterceptorTableRow::TimeRegisteredFormattingOptions))
				];
		}
		else if (ColumnName == "TimeUnregistered")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(this, &SMessagingInterceptorsTableRow::HandleTimeUnregisteredText)
				];
		}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:

	/** Callback for getting the timestamp at which the address was unregistered. */
	FText HandleTimeUnregisteredText() const
	{
		if (InterceptorInfo->TimeUnregistered > 0.0)
		{
			return FText::AsNumber(InterceptorInfo->TimeUnregistered - GStartTime, &MessagingInterceptorTableRow::TimeRegisteredFormattingOptions);
		}

		return LOCTEXT("Never", "Never");
	}

private:

	/** Holds the interceptor information. */
	FMessageTracerInterceptorInfoPtr InterceptorInfo;

	/** Holds a pointer to the view model. */
	FMessagingDebuggerModelPtr Model;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;
};


#undef LOCTEXT_NAMESPACE
