// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MessagingDebuggerPrivatePCH.h"
#include "SMessagingTypesFilterBar.h"
#include "SSearchBox.h"


#define LOCTEXT_NAMESPACE "SMessagingTypesFilterBar"


/* SMessagingTypesFilterBar interface
*****************************************************************************/

void SMessagingTypesFilterBar::Construct(const FArguments& InArgs, FMessagingDebuggerTypeFilterRef InFilter)
{
	Filter = InFilter;

	ChildSlot
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Top)
			[
				// search box
				SNew(SSearchBox)
				.HintText(LOCTEXT("SearchBoxHint", "Search message types"))
				.OnTextChanged(this, &SMessagingTypesFilterBar::HandleFilterStringTextChanged)
			]
		];
}

#undef LOCTEXT_NAMESPACE
