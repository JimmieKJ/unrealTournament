// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SessionFrontendPrivatePCH.h"


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
