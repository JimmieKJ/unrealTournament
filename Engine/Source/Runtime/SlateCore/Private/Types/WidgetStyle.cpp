// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"


/* Static initialization
 *****************************************************************************/

const float FWidgetStyle::SubdueAmount = 0.6f;


/* FWidgetStyle interface
 *****************************************************************************/

FWidgetStyle& FWidgetStyle::SetForegroundColor( const TAttribute<struct FSlateColor>& InForeground )
{
	ForegroundColor = InForeground.Get().GetColor(*this);

	SubduedForeground = ForegroundColor;
	SubduedForeground.A *= SubdueAmount;

	return *this;
}
