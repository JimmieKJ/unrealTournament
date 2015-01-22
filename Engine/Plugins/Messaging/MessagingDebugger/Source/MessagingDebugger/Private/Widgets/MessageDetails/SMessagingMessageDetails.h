// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the message details panel.
 */
class SMessagingMessageDetails
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMessagingMessageDetails) { }
	SLATE_END_ARGS()

public:

	/** Destructor. */
	~SMessagingMessageDetails();

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InModel The view model to use.
	 * @param InStyle The visual style to use for this widget.
	 */
	void Construct( const FArguments& InArgs, const FMessagingDebuggerModelRef& InModel, const TSharedRef<ISlateStyle>& InStyle );

protected:

	/** Refreshes the details widget. */
	void RefreshDetails();

private:

	/** Callback for generating a row widget for the dispatch state list view. */
	TSharedRef<ITableRow> HandleDispatchStateListGenerateRow( FMessageTracerDispatchStatePtr DispatchState, const TSharedRef<STableViewBase>& OwnerTable );

	/** Callback for getting the text of the 'Expiration' field. */
	FText HandleExpirationText() const;

	/** Callback for handling message selection changes. */
	void HandleModelSelectedMessageChanged();

	/** Callback for getting the text of the 'Sender Thread' field. */
	FText HandleSenderThreadText() const;

	/** Callback for getting the text of the 'Timestamp' field. */
	FText HandleTimestampText() const;

private:

	/** Holds the list of dispatch states. */
	TArray<FMessageTracerDispatchStatePtr> DispatchStateList;

	/** Holds the dispatch state list view. */
	TSharedPtr<SListView<FMessageTracerDispatchStatePtr> > DispatchStateListView;

	/** Holds a pointer to the view model. */
	FMessagingDebuggerModelPtr Model;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;
};
