// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Endpoints/SMessagingEndpointsFilterBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SSearchBox.h"


#define LOCTEXT_NAMESPACE "SMessagingEndpointsFilterBar"


/* SMessagingEndpointsFilterBar interface
*****************************************************************************/

void SMessagingEndpointsFilterBar::Construct(const FArguments& InArgs, TSharedRef<FMessagingDebuggerEndpointFilter> InFilter)
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
					.OnTextChanged_Lambda([this](const FText& NewText) {
						Filter->SetFilterString(NewText.ToString());
					})
			]
	];
}


#undef LOCTEXT_NAMESPACE
