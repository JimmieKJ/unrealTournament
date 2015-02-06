// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"
#include "SCompoundWidget.h"
#include "SWindow.h"

class STutorialLoading : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(STutorialLoading) {}
	
	SLATE_ARGUMENT(TWeakPtr<SWindow>, ContextWindow)
	
	SLATE_END_ARGS()

public:

	/** Widget constructor */
	void Construct(const FArguments& InArgs);

private:
	/** Window where loading visual will be displayed */
	TWeakPtr<SWindow> ContextWindow;
};