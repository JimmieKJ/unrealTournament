// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Models/MessagingDebuggerEndpointFilter.h"

/**
 * Implements the endpoints list filter bar widget.
 */
class SMessagingEndpointsFilterBar
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMessagingEndpointsFilterBar) { }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InFilter The filter model.
	 */
	void Construct(const FArguments& InArgs, TSharedRef<FMessagingDebuggerEndpointFilter> InFilter);

private:

	/** Holds a pointer to the filter model. */
	TSharedPtr<FMessagingDebuggerEndpointFilter> Filter;
};
