// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MessagingDebuggerPrivatePCH.h"
#include "SMessagingEndpointsFilterBar.h"
#include "SSearchBox.h"


#define LOCTEXT_NAMESPACE "SMessagingEndpointsFilterBar"


/* SMessagingEndpointsFilterBar interface
*****************************************************************************/

void SMessagingEndpointsFilterBar::Construct(const FArguments& InArgs, FMessagingDebuggerEndpointFilterRef InFilter)
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
				.HintText(LOCTEXT("SearchBoxHint", "Search endpoints"))
				.OnTextChanged(this, &SMessagingEndpointsFilterBar::HandleFilterStringTextChanged)
			]
		];
}


#undef LOCTEXT_NAMESPACE
