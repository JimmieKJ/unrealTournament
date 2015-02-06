// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SMessagingEndpointsTableRow"


/**
 * Implements a row widget for the session console log.
 */
class SMessagingEndpointsTableRow
	: public SMultiColumnTableRow<FMessageTracerEndpointInfoPtr>
{
public:

	SLATE_BEGIN_ARGS(SMessagingEndpointsTableRow) { }
		SLATE_ATTRIBUTE(FText, HighlightText)
		SLATE_ARGUMENT(FMessageTracerEndpointInfoPtr, EndpointInfo)
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
		check(InArgs._EndpointInfo.IsValid());
		check(InArgs._Style.IsValid());

		EndpointInfo = InArgs._EndpointInfo;
		Model = InModel;
		HighlightText = InArgs._HighlightText;
		Style = InArgs._Style;

		SMultiColumnTableRow<FMessageTracerEndpointInfoPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

public:

	// SMultiColumnTableRow interface

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		if (ColumnName == "Break")
		{
			return SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.33f))
				.BorderImage(Style->GetBrush("BreakpointBorder"));

/*			return SNew(SImage)
				.Image(Style->GetBrush("BreakDisabled"));*/
		}
		else if (ColumnName == "Name")
		{
			return SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(FMargin(4.0f, 0.0f))
					[
						SNew(SImage)
							.Image(Style->GetBrush(EndpointInfo->Remote ? "RemoteEndpoint" : "LocalEndpoint"))
							.ToolTipText(EndpointInfo->Remote ? LOCTEXT("RemoteEndpointTooltip", "Remote Endpoint") : LOCTEXT("LocalEndpointTooltip", "Local Endpoint"))
					]

				+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.HighlightText(HighlightText)
							.Text(FText::FromName(EndpointInfo->Name))
					];
		}
		else if (ColumnName == "Messages")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.HighlightText(HighlightText)
						.Text(this, &SMessagingEndpointsTableRow::HandleMessagesText)
						.ToolTipText(this, &SMessagingEndpointsTableRow::HandleMessagesTooltipText)
				];
		}
		else if (ColumnName == "Visibility")
		{
			return SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
						.Style(&Style->GetWidgetStyle<FCheckBoxStyle>("VisibilityCheckbox"))
						.IsChecked(this, &SMessagingEndpointsTableRow::HandleVisibilityCheckBoxIsChecked)
						.OnCheckStateChanged(this, &SMessagingEndpointsTableRow::HandleVisibilityCheckBoxCheckStateChanged)
						.ToolTipText(LOCTEXT("VisibilityCheckboxTooltipText", "Toggle visibility of messages from or to this endpoint"))
				];
		}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:

	/** Gets the text for the Messages column. */
	FText HandleMessagesText() const
	{
		return FText::AsNumber(EndpointInfo->ReceivedMessages.Num() + EndpointInfo->SentMessages.Num());
	}

	/** Gets the tooltip text for the Messages column. */
	FText HandleMessagesTooltipText() const
	{
		return FText::Format(LOCTEXT("MessagesTooltipTextFmt", "In: {0}\nOut: {1}"), FText::AsNumber(EndpointInfo->ReceivedMessages.Num()), FText::AsNumber(EndpointInfo->SentMessages.Num()));
	}

	/** Handles changing the checked state of the visibility check box. */
	void HandleVisibilityCheckBoxCheckStateChanged( ECheckBoxState CheckState )
	{
		Model->SetEndpointVisibility(EndpointInfo.ToSharedRef(), (CheckState == ECheckBoxState::Checked));
	}

	/** Gets the image for the visibility check box. */
	ECheckBoxState HandleVisibilityCheckBoxIsChecked() const
	{
		if (Model->IsEndpointVisible(EndpointInfo.ToSharedRef()))
		{
			return ECheckBoxState::Checked;
		}

		return ECheckBoxState::Unchecked;
	}

private:

	/** Holds the endpoint's debug information. */
	FMessageTracerEndpointInfoPtr EndpointInfo;

	/** Holds the highlight string for the message. */
	TAttribute<FText> HighlightText;

	/** Holds a pointer to the view model. */
	FMessagingDebuggerModelPtr Model;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;
};


#undef LOCTEXT_NAMESPACE
