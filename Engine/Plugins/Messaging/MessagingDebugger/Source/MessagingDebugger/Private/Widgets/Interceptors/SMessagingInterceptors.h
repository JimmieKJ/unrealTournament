// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the message interceptors panel.
 */
class SMessagingInterceptors
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMessagingInterceptors) { }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InModel The view model to use.
	 * @param InStyle The visual style to use for this widget.
	 * @param InTracer The message tracer.
	 */
	void Construct(const FArguments& InArgs, const FMessagingDebuggerModelRef& InModel, const TSharedRef<ISlateStyle>& InStyle, const IMessageTracerRef& InTracer);

protected:

	/** Reloads the list of interceptors. */
	void ReloadInterceptorList();

private:

	/** Handles generating a row widget for the interceptor list view. */
	TSharedRef<ITableRow> HandleInterceptorListGenerateRow(FMessageTracerInterceptorInfoPtr InterceptorInfo, const TSharedRef<STableViewBase>& OwnerTable);

private:

	/** Holds the filtered list of interceptors. */
	TArray<FMessageTracerInterceptorInfoPtr> InterceptorList;

	/** Holds the interceptor list view. */
	TSharedPtr<SListView<FMessageTracerInterceptorInfoPtr>> InterceptorListView;

	/** Holds a pointer to the view model. */
	FMessagingDebuggerModelPtr Model;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;

	/** Holds a pointer to the message bus tracer. */
	IMessageTracerPtr Tracer;
};
