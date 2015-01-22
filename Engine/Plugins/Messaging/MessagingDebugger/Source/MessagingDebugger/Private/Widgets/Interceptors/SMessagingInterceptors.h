// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	 * @param InStyle The visual style to use for this widget.
	 * @param InTracer The message tracer.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<ISlateStyle>& InStyle, const IMessageTracerRef& InTracer );
};
