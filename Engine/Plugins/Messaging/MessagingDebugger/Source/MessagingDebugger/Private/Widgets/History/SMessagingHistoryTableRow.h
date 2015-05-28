// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SMessagingHistoryTableRow"


/**
 * Implements a row widget for the message history list.
 */
class SMessagingHistoryTableRow
	: public SMultiColumnTableRow<FMessageTracerMessageInfoPtr>
{
public:

	SLATE_BEGIN_ARGS(SMessagingHistoryTableRow) { }
		SLATE_ATTRIBUTE(FText, HighlightText)
		SLATE_ARGUMENT(FMessageTracerMessageInfoPtr, MessageInfo)
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
		check(InArgs._MessageInfo.IsValid());
		check(InArgs._Style.IsValid());

		MaxDispatchLatency = -1.0;
		MaxHandlingTime = -1.0;
		HighlightText = InArgs._HighlightText;
		MessageInfo = InArgs._MessageInfo;
		Style = InArgs._Style;

		SMultiColumnTableRow<FMessageTracerMessageInfoPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

public:

	// SWidget overrides

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override
	{
		MaxDispatchLatency = -1.0;
		MaxHandlingTime = -1.0;

		for (TMap<FMessageTracerEndpointInfoPtr, FMessageTracerDispatchStatePtr>::TConstIterator It(MessageInfo->DispatchStates); It; ++It)
		{
			const FMessageTracerDispatchStatePtr& DispatchState = It.Value();

			if (MessageInfo->TimeRouted > 0.0)
			{
				MaxDispatchLatency = FMath::Max(MaxDispatchLatency, DispatchState->DispatchLatency);
			}

			if (DispatchState->TimeHandled > 0.0)
			{
				MaxHandlingTime = FMath::Max(MaxHandlingTime, DispatchState->TimeHandled - DispatchState->TimeDispatched);
			}
		}
	}

public:

	// SMultiColumnTableRow interface

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		if (ColumnName == "DispatchLatency")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SMessagingHistoryTableRow::HandleTextColorAndOpacity)
						.Text(this, &SMessagingHistoryTableRow::HandleDispatchLatencyText)
				];
		}
		else if (ColumnName == "Flag")
		{
			return SNew(SBox)
				.ToolTipText(LOCTEXT("DeadMessageTooltip", "No valid recipients (dead letter)"))
				[
					SNew(SImage)
						.Image(this, &SMessagingHistoryTableRow::HandleFlagImage)
				];
		}
		else if (ColumnName == "HandleTime")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SMessagingHistoryTableRow::HandleTextColorAndOpacity)
						.Text(this, &SMessagingHistoryTableRow::HandleHandlingTimeText)
				];
		}
		else if (ColumnName == "MessageType")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SMessagingHistoryTableRow::HandleTextColorAndOpacity)
						.HighlightText(HighlightText)
						.Text(FText::FromName(MessageInfo->Context->GetMessageType()))
				];
		}
		else if (ColumnName == "Recipients")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.ToolTipText(LOCTEXT("LocalRemoteRecipients", "Local/Remote Recipients"))
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SMessagingHistoryTableRow::HandleTextColorAndOpacity)
						.Text(this, &SMessagingHistoryTableRow::HandleRecipientsText)
				];
		}
		else if (ColumnName == "RouteLatency")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SMessagingHistoryTableRow::HandleRouteLatencyColorAndOpacity)
						.Text(this, &SMessagingHistoryTableRow::HandleRouteLatencyText)
				];
		}
		else if (ColumnName == "Scope")
		{
			FText Text;
			FText ToolTipText;

			if (MessageInfo->Context.IsValid())
			{
				int32 NumRecipients = MessageInfo->Context->GetRecipients().Num();

				if (MessageInfo->Context->IsForwarded())
				{
					// forwarded message
					FText ScopeText = ScopeToText(MessageInfo->Context->GetScope());
					Text = FText::Format(LOCTEXT("ForwardedMessageTextFormat", "F - {0}"), ScopeText);

					if (NumRecipients > 0)
					{
						ToolTipText = FText::Format(LOCTEXT("ForwardedSentMessageToolTipTextFormat", "This message was forwarded directly to {0} recipients"), FText::AsNumber(NumRecipients));
					}
					else
					{
						ToolTipText = FText::Format(LOCTEXT("ForwardedPublishedMessageToolTipTextFormat", "This message was forwarded to all subscribed recipients in scope '{0}'"), ScopeText);
					}				
				}
				else if (NumRecipients == 0)
				{
					// published message
					FText ScopeText = ScopeToText(MessageInfo->Context->GetScope());

					Text = FText::Format(LOCTEXT("PublishedMessageTextFormat", "P - {0}"), ScopeText);
					ToolTipText = FText::Format(LOCTEXT("PublishedMessageToolTipTextFormat", "This message was published to all subscribed recipients in scope '{0}'"), ScopeText);
				}
				else
				{
					// sent message
					Text = LOCTEXT("SentMessageTextFormat", "S");
					ToolTipText = FText::Format(LOCTEXT("SentMessageToolTipTextFormat", "This message was sent directly to {0} recipients"), FText::AsNumber(NumRecipients));
				}
			}

			return SNew(SBox)
				.ToolTipText(ToolTipText)
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SMessagingHistoryTableRow::HandleTextColorAndOpacity)
						.Text(Text)
				];
		}
		else if (ColumnName == "Sender")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SMessagingHistoryTableRow::HandleTextColorAndOpacity)
						.HighlightText(HighlightText)
						.Text(this, &SMessagingHistoryTableRow::HandleSenderText)
				];
		}
		else if (ColumnName == "TimeSent")
		{
			FNumberFormattingOptions NumberFormattingOptions;
			NumberFormattingOptions.MaximumFractionalDigits = 3;
			NumberFormattingOptions.MinimumFractionalDigits = 3;

			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SMessagingHistoryTableRow::HandleTextColorAndOpacity)
						.HighlightText(HighlightText)
						.Text(FText::AsNumber(MessageInfo->TimeSent - GStartTime, &NumberFormattingOptions))
				];
		}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

protected:

	/** 
	 * Converts the given message scope to a human readable string.
	 *
	 * @return The string representation.
	 */
	FText ScopeToText( EMessageScope Scope ) const
	{
		switch (Scope)
		{
		case EMessageScope::Thread:
			return LOCTEXT("ScopeThread", "Thread");
			break;

		case EMessageScope::Process:
			return LOCTEXT("ScopeProcess", "Process");
			break;

		case EMessageScope::Network:
			return LOCTEXT("ScopeNetwork", "Network");
			break;

		case EMessageScope::All:
			return LOCTEXT("ScopeAll", "All");
			break;

		default:
			return LOCTEXT("ScopeUnknown", "Unknown");
		}
	}

	/**
	 * Converts the given time span in seconds to a human readable string.
	 *
	 * @param Seconds The time span to convert.
	 * @return The string representation.
	 *
	 * @todo gmp: refactor this into FText::AsTimespan or something like that
	 */
	FText TimespanToReadableText( double Seconds ) const
	{
		if (Seconds < 0.0)
		{
			return LOCTEXT("Zero Length Timespan", "-");
		}

		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 1;
		Options.MaximumFractionalDigits = 1;

		if (Seconds < 0.0001)
		{
			return FText::Format(LOCTEXT("Seconds < 0.0001 Length Timespan", "{0} us"), FText::AsNumber(Seconds * 1000000, &Options));
		}

		if (Seconds < 0.1)
		{
			return FText::Format(LOCTEXT("Seconds < 0.1 Length Timespan", "{0} ms"), FText::AsNumber(Seconds * 1000, &Options));
		}

		if (Seconds < 60.0)
		{
			return FText::Format(LOCTEXT("Seconds < 60.0 Length Timespan", "{0} s"), FText::AsNumber(Seconds, &Options));
		}
		
		return LOCTEXT("> 1 minute Length Timespan", "> 1 min");
	}

private:

	/** Callback for getting the text color of the DispatchLatency column. */
	FSlateColor HandleDispatchLatencyColorAndOpacity() const
	{
		if (MaxDispatchLatency >= 0.01)
		{
			return FLinearColor::Red;
		}

		if (MaxDispatchLatency >= 0.001)
		{
			return FLinearColor(1.0f, 1.0f, 0.0f);
		}

		if (MaxDispatchLatency >= 0.0001)
		{
			return FLinearColor::Yellow;
		}

		return FSlateColor::UseForeground();
	}

	/** Callback for getting the text in the DispatchLatency column. */
	FText HandleDispatchLatencyText() const
	{
		return TimespanToReadableText(MaxDispatchLatency);
	}

	const FSlateBrush* HandleFlagImage() const
	{
		if ((MessageInfo->TimeRouted > 0.0) && (MessageInfo->DispatchStates.Num() == 0))
		{
			return Style->GetBrush("DeadMessage");
		}

		return NULL;
	}

	/** Callback for getting the text in the HandlingTime column. */
	FText HandleHandlingTimeText() const
	{
		return TimespanToReadableText(MaxHandlingTime);
	}

	/** Callback for getting the text in the Recipients column. */
	FText HandleRecipientsText() const
	{
		int32 LocalRecipients = 0;
		int32 RemoteRecipients = 0;

		for (auto It = MessageInfo->DispatchStates.CreateConstIterator(); It; ++It)
		{
			FMessageTracerEndpointInfoPtr EndpointInfo = It.Value()->EndpointInfo;

			if (EndpointInfo.IsValid())
			{
				if (EndpointInfo->Remote)
				{
					++RemoteRecipients;
				}
				else
				{
					++LocalRecipients;
				}
			}
		}

		return FText::Format(LOCTEXT("RecipientsTextFormat", "{0} / {1}"), FText::AsNumber(LocalRecipients), FText::AsNumber(RemoteRecipients));
	}

	/** Callback for getting the text color of the RouteLatency column. */
	FSlateColor HandleRouteLatencyColorAndOpacity() const
	{
		double RouteLatency = MessageInfo->TimeRouted - MessageInfo->TimeSent;

		if (RouteLatency >= 0.01)
		{
			return FLinearColor::Red;
		}

		if (RouteLatency >= 0.001)
		{
			return FLinearColor(1.0f, 1.0f, 0.0f);
		}

		if (RouteLatency >= 0.0001)
		{
			return FLinearColor::Yellow;
		}

		if (MessageInfo->TimeRouted == 0.0)
		{
			return FSlateColor::UseSubduedForeground();
		}

		return FSlateColor::UseForeground();
	}

	/** Callback for getting the text in the RouteLatency column. */
	FText HandleRouteLatencyText() const
	{
		if (MessageInfo->TimeRouted > 0.0)
		{
			return TimespanToReadableText(MessageInfo->TimeRouted - MessageInfo->TimeSent);
		}

		return LOCTEXT("RouteLatencyPending", "Pending");
	}

	/** Callback for getting the text in the Sender column. */
	FText HandleSenderText() const
	{
		return FText::FromString(MessageInfo->SenderInfo->Name.ToString());
	}

	/** Callback for getting the text color of various columns. */
	FSlateColor HandleTextColorAndOpacity() const
	{
		if (MessageInfo->TimeRouted == 0.0)
		{
			return FSlateColor::UseSubduedForeground();
		}

		return FSlateColor::UseForeground();
	}

private:

	/** Holds the highlight string for the message. */
	TAttribute<FText> HighlightText;

	/** Holds message's debug information. */
	FMessageTracerMessageInfoPtr MessageInfo;

	/** Holds the maximum dispatch latency. */
	double MaxDispatchLatency;

	/** Holds the maximum time that was needed to handle the message. */
	double MaxHandlingTime;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;
};


#undef LOCTEXT_NAMESPACE
