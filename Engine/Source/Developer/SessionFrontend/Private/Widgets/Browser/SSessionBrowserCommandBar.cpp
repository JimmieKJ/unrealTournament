// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Browser/SSessionBrowserCommandBar.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "SSessionBrowserCommandBar"


/* SSessionBrowserCommandBar interface
 *****************************************************************************/

void SSessionBrowserCommandBar::Construct( const FArguments& InArgs )
{
	ChildSlot
	[
		SNew(SHorizontalBox)
	];
}


#undef LOCTEXT_NAMESPACE
