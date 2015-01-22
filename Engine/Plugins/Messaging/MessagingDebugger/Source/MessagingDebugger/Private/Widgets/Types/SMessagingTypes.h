// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the message types panel.
 */
class SMessagingTypes
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMessagingTypes) { }
	SLATE_END_ARGS()

public:

	/** Destructor. */
	~SMessagingTypes();

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InModel The view model to use.
	 * @param InStyle The visual style to use for this widget.
	 * @param InTracer The message tracer.
	 */
	void Construct( const FArguments& InArgs, const FMessagingDebuggerModelRef& InModel, const TSharedRef<ISlateStyle>& InStyle, const IMessageTracerRef& InTracer );

protected:

	/**
	 * Adds the given message to the history.
	 *
	 * @param TypeInfo The information about the message type to add.
	 */
	void AddType( const FMessageTracerTypeInfoRef& TypeInfo );

	/** Reloads the message history. */
	void ReloadTypes();

private:

	/** Callback for message type filter changes. */
	void HandleFilterChanged();

	/** Callback for handling message selection changes. */
	void HandleModelSelectedMessageChanged();

	/** Callback for when the tracer's message history has been reset. */
	void HandleTracerMessagesReset();

	/** Callback for when a message type has been added to the message tracer. */
	void HandleTracerTypeAdded( FMessageTracerTypeInfoRef TypeInfo );

	/** Callback for generating a row widget for the message type list view. */
	TSharedRef<ITableRow> HandleTypeListGenerateRow( FMessageTracerTypeInfoPtr TypeInfo, const TSharedRef<STableViewBase>& OwnerTable );

	/** Callback for getting the highlight string for message types. */
	FText HandleTypeListGetHighlightText() const;

	/** Callback for selecting message types. */
	void HandleTypeListSelectionChanged( FMessageTracerTypeInfoPtr InItem, ESelectInfo::Type SelectInfo );

private:

	/** Holds the message type filter model. */
	FMessagingDebuggerTypeFilterPtr Filter;

	/** Holds a pointer to the view model. */
	FMessagingDebuggerModelPtr Model;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;

	/** Holds a pointer to the message bus tracer. */
	IMessageTracerPtr Tracer;

	/** Holds the filtered list of message types. */
	TArray<FMessageTracerTypeInfoPtr> TypeList;

	/** Holds the message type list view. */
	TSharedPtr<SListView<FMessageTracerTypeInfoPtr>> TypeListView;
};
