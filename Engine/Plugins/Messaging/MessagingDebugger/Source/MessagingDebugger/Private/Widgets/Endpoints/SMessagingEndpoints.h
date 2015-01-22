// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the message endpoints panel.
 */
class SMessagingEndpoints
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMessagingEndpoints) { }
	SLATE_END_ARGS()

public:

	/**  Destructor. */
	~SMessagingEndpoints();

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

public:

	// SCompoundWidget overrides

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

protected:

	/** Reloads the list of endpoints. */
	void ReloadEndpointList();

private:

	/** Handles generating a row widget for the endpoint list view. */
	TSharedRef<ITableRow> HandleEndpointListGenerateRow( FMessageTracerEndpointInfoPtr EndpointInfo, const TSharedRef<STableViewBase>& OwnerTable );

	/** Handles getting the highlight string for endpoints. */
	FText HandleEndpointListGetHighlightText() const;

	/** Handles the selection of endpoints. */
	void HandleEndpointListSelectionChanged( FMessageTracerEndpointInfoPtr InItem, ESelectInfo::Type SelectInfo );

	/** Handles endpoint filter changes. */
	void HandleFilterChanged();

	/** Callback for handling message selection changes. */
	void HandleModelSelectedMessageChanged();

private:

	/** Holds the filter bar. */
	//TSharedPtr<SMessagingEndpointsFilterBar> FilterBar;

	/** Holds the filtered list of historic messages. */
	TArray<FMessageTracerEndpointInfoPtr> EndpointList;

	/** Holds the message list view. */
	TSharedPtr<SListView<FMessageTracerEndpointInfoPtr>> EndpointListView;

	/** Holds the endpoint filter model. */
	FMessagingDebuggerEndpointFilterPtr Filter;

	/** Holds a pointer to the view model. */
	FMessagingDebuggerModelPtr Model;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;

	/** Holds a pointer to the message bus tracer. */
	IMessageTracerPtr Tracer;
};
