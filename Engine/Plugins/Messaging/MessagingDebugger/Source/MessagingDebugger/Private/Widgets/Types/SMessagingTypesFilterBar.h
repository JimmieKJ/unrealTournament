// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Models/MessagingDebuggerTypeFilter.h"

/**
 * Implements the message type list filter bar widget.
 */
class SMessagingTypesFilterBar
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMessagingTypesFilterBar) { }

		/** Called when the filter settings have changed. */
		SLATE_EVENT(FSimpleDelegate, OnFilterChanged)

	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InFilter The filter model.
	 */
	void Construct( const FArguments& InArgs, TSharedRef<FMessagingDebuggerTypeFilter> InFilter );

private:

	/** Holds the filter model. */
	TSharedPtr<FMessagingDebuggerTypeFilter> Filter;
};
